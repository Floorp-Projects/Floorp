/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 *  editordialogs.c --- Editor-specific dialogs.
 *  Should only be built for the editor.
 *  Created: David Williams <djw@netscape.com>, Mar-12-1996
 *
 *  RCSID: "$Id: editordialogs.c,v 3.7 1998/10/08 21:36:20 akkana%netscape.com Exp $"
 */

#include "mozilla.h"
#include "xfe.h"
#include <X11/keysym.h>   /* for editor key translation */
#include <XmL/Folder.h>   /* tab folder stuff */
#include <Xm/Label.h> 
#include <Xm/DrawnB.h> 
#include <Xm/XmP.h>       /* required for _XmFontListGetDefaultFont() */
#include "DtWidgets/ComboBox.h"
#include <xpgetstr.h>     /* for XP_GetString() */

#include <Xfe/Xfe.h>		/* for xfe widgets and utilities */

#ifdef EDITOR

#include "edttypes.h"
#include "edt.h"
#include "menu.h"
#include "xeditor.h"
#include "il_icons.h"           /* Image icon enumeration. */
#include "prefapi.h"   /* Need some publishing prefs for EDT_PublishFile */
#include "felocale.h"

/* no security btn on prefs
#define _SECURITY_BTN_ON_PREFS
*/

/*
 *    I don't like this "fix" for MAXPATHLEN, but I cannot log onto
 *    a SCO machine to find the right way to do this...djw
 */
#ifdef SCO_SV
#include "mcom_db.h"	/* for MAXPATHLEN */
#endif

void fe_browse_file_of_text (MWContext *context, Widget text_field, Boolean dirp);

extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_VALUES_OUT_OF_RANGE;
extern int XFE_VALUE_OUT_OF_RANGE;
extern int XFE_ENTER_NEW_VALUE;
extern int XFE_ENTER_NEW_VALUES;
extern int XFE_EDITOR_LINK_TEXT_LABEL_NEW;
extern int XFE_EDITOR_LINK_TEXT_LABEL_IMAGE;
extern int XFE_EDITOR_LINK_TEXT_LABEL_TEXT;
extern int XFE_EDITOR_LINK_TARGET_LABEL_NO_TARGETS;
extern int XFE_EDITOR_LINK_TARGET_LABEL_SPECIFIED;
extern int XFE_EDITOR_LINK_TARGET_LABEL_CURRENT;
extern int XFE_EDITOR_WARNING_REMOVE_LINK;
extern int XFE_UNKNOWN;
extern int XFE_EDITOR_TAG_UNOPENED;
extern int XFE_EDITOR_TAG_UNCLOSED;
extern int XFE_EDITOR_TAG_UNTERMINATED_STRING;
extern int XFE_EDITOR_TAG_PREMATURE_CLOSE;
extern int XFE_EDITOR_TAG_TAGNAME_EXPECTED;
extern int XFE_EDITOR_TAG_UNKNOWN;
extern int XFE_EDITOR_TAG_OK;
extern int XFE_EDITOR_AUTOSAVE_PERIOD_RANGE;
extern int XFE_EDITOR_PUBLISH_LOCATION_INVALID;
extern int XFE_EDITOR_IMAGE_IS_REMOTE;
extern int XFE_CANNOT_READ_FILE;

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

extern int XP_EDT_CHARSET_CONVERT_PAGE;
extern int XP_EDT_CHARSET_SET_METATAG;

#define IMAGE_MIN_WIDTH  1
#define IMAGE_MAX_WIDTH  10000
#define IMAGE_MIN_HEIGHT 1
#define IMAGE_MAX_HEIGHT 10000
#define IMAGE_MIN_HSPACE 0
#define IMAGE_MAX_HSPACE 10000
#define IMAGE_MIN_VSPACE 0
#define IMAGE_MAX_VSPACE 10000
#define IMAGE_MIN_BORDER 0
#define IMAGE_MAX_BORDER 10000

#define TABLE_MAX_PERCENT_WIDTH 100
#define TABLE_MIN_PERCENT_WIDTH 1
#define TABLE_MAX_PERCENT_HEIGHT 100
#define TABLE_MIN_PERCENT_HEIGHT 1

#define HRULE_MIN_HEIGHT 1
#define HRULE_MAX_HEIGHT 10000
#define HRULE_MAX_PIXEL_WIDTH  10000
#define HRULE_MIN_PIXEL_WIDTH  1
#define HRULE_MAX_PERCENT_WIDTH 100
#define HRULE_MIN_PERCENT_WIDTH 1

#define AUTOSAVE_MIN_PERIOD 0
#define AUTOSAVE_MAX_PERIOD 600

#define RANGE_CHECK(o, a, b) ((o) < (a) || (o) > (b))

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

#define XFE_INVALID_IMAGE_WIDTH     12
#define XFE_INVALID_IMAGE_HEIGHT    13
#define XFE_INVALID_IMAGE_HSPACE    14
#define XFE_INVALID_IMAGE_VSPACE    15
#define XFE_INVALID_IMAGE_BORDER    16

#define XFE_INVALID_HRULE_WIDTH     17
#define XFE_INVALID_HRULE_HEIGHT    18

static void
fe_error_dialog(MWContext* context, Widget parent, char* s)
{
	while (!XtIsWMShell(parent) && (XtParent(parent)!=0))
		parent = XtParent(parent);

	fe_dialog(parent, "error", s, FALSE, 0, FALSE, FALSE, 0);
}

static void
fe_message_dialog(MWContext* context, Widget parent, char* s)
{
	Widget mainw;

	while (!XtIsWMShell(parent) && (XtParent(parent)!=0))
		parent = XtParent(parent);

	/* yuck */
	mainw = CONTEXT_WIDGET(context);
	CONTEXT_WIDGET(context) = parent;

	fe_Message(context, s);

	CONTEXT_WIDGET(context) = mainw;
}

void
fe_editor_range_error_dialog(MWContext* context, Widget parent,
							 unsigned* errors, unsigned nerrors)
{
	unsigned i;
	int      id;
	char     be_lazy[8192];

	if (nerrors > 1)
		id = XFE_VALUES_OUT_OF_RANGE; /* Some values are out of range: */
	else if (nerrors == 1)
		id = XFE_VALUE_OUT_OF_RANGE; /* The following value is out of range: */
	else
		return;

	strcpy(be_lazy, XP_GetString(id));
	strcat(be_lazy, "\n\n");

	for (i = 0; i < nerrors; i++) {
		switch (errors[i]) {
		case XFE_INVALID_TABLE_NROWS:
		case XFE_INVALID_CELL_NROWS:
			id = XFE_EDITOR_TABLE_ROW_RANGE;
			/* You can have between 1 and 100 rows. */
			break;
		case XFE_INVALID_TABLE_NCOLUMNS:
		case XFE_INVALID_CELL_NCOLUMNS:
			id = XFE_EDITOR_TABLE_COLUMN_RANGE;
			/* You can have between 1 and 100 columns. */
			break;
		case XFE_INVALID_TABLE_BORDER:
			id = XFE_EDITOR_TABLE_BORDER_RANGE;
			/* For border width, you can have 0 to 10000 pixels. */
			break;
		case XFE_INVALID_TABLE_SPACING:
			id = XFE_EDITOR_TABLE_SPACING_RANGE;
			/* For cell spacing, you can have 0 to 10000 pixels. */
			break;
		case XFE_INVALID_TABLE_PADDING:
			id = XFE_EDITOR_TABLE_PADDING_RANGE;
			/* For cell padding, you can have 0 to 10000 pixels. */
			break;
		case XFE_INVALID_TABLE_WIDTH:
		case XFE_INVALID_CELL_WIDTH:
		case XFE_INVALID_HRULE_WIDTH:
			id = XFE_EDITOR_TABLE_WIDTH_RANGE;
			/* For width, you can have between 1 and 10000 pixels, */
			/* or between 1 and 100%.                              */
			break;
		case XFE_INVALID_TABLE_HEIGHT:
		case XFE_INVALID_CELL_HEIGHT:
			id = XFE_EDITOR_TABLE_HEIGHT_RANGE;
			/* For height, you can have between 1 and 10000 pixels, */
			/* or between 1 and 100%.                               */
			break;
		case XFE_INVALID_IMAGE_WIDTH:
			id = XFE_EDITOR_TABLE_IMAGE_WIDTH_RANGE;
			/* For width, you can have between 1 and 10000 pixels. */
			break;
		case XFE_INVALID_IMAGE_HEIGHT:
		case XFE_INVALID_HRULE_HEIGHT:
			id = XFE_EDITOR_TABLE_IMAGE_HEIGHT_RANGE;
			/* For height, you can have between 1 and 10000 pixels. */
			break;
		case XFE_INVALID_IMAGE_HSPACE:
		case XFE_INVALID_IMAGE_VSPACE:
		case XFE_INVALID_IMAGE_BORDER:
			id = XFE_EDITOR_TABLE_IMAGE_SPACE_RANGE;
			/* For space, you can have between 1 and 10000 pixels. */
			break;
		}

		strcat(be_lazy, XP_GetString(id));
		strcat(be_lazy, "\n");
	}

	if (nerrors > 1) {
		id = XFE_ENTER_NEW_VALUES;
		/* Please enter new values and try again. */
    } else if (nerrors == 1) {
		id = XFE_ENTER_NEW_VALUE;
		/* Please enter a new value and try again. */
	}

	strcat(be_lazy, "\n");
	strcat(be_lazy, XP_GetString(id));

	fe_error_dialog(context, parent, be_lazy);
}

#endif  /* EDITOR */

Widget
fe_CreateTabForm(Widget parent, char* name, Arg* args, Cardinal n)
{
	Widget form;
	Widget tab;
	Pixel  bg;
	char*    string;
	XmString xm_string;

	XtVaGetValues(parent, XmNbackground, &bg, 0);
	form = XtVaCreateWidget(name, xmFormWidgetClass, parent, XmNbackground, bg, NULL);

	string = XfeSubResourceGetWidgetStringValue(form, "tabLabelString", XmCXmString);
	if (!string)
		string = name;
	xm_string = XmStringCreateSimple(string);
	tab = XmLFolderAddTab(parent, xm_string);
	XtVaSetValues(tab, XmNtabManagedWidget, form, NULL);

	return form;
}

#ifdef EDITOR

void
fe_WidgetSetSensitive(Widget widget, Boolean sensitive)
{
    XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}

/* This routine has a name conflict with another fe_CreateSwatch
 * in colorpicker.c.  The one in colorpicker.c looks much more
 * elaborate, so I'm choosing to rename this one.       ...Akkana
 */
#define SWATCH_SIZE 60

Widget
fe_CreateSwatchButton(Widget parent, char* name, Arg* p_args, Cardinal p_n)
{
    return XmCreateDrawnButton(parent, name, p_args, p_n);
}

void
fe_SwatchSetSensitive(Widget widget, Boolean sensitive)
{
    XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}

#endif  /* EDITOR */

void
fe_SwatchSetColor(Widget widget, LO_Color* color)
{
	MWContext* context = fe_WidgetToMWContext(widget);
	Pixel      pixel;

	if (color != NULL) {
		pixel = fe_GetPixel(context, color->red, color->green, color->blue);
	} else {
		XtVaGetValues(XtParent(widget), XmNbackground, &pixel, 0);
	}
	
	XtVaSetValues(widget, XmNbackground, pixel, 0);
}

#ifdef EDITOR

Widget
fe_CreatePasswordField(Widget parent, char* name, Arg* args, Cardinal n)
{
	Widget widget;
	int    max_length;

	widget = fe_CreateTextField(parent, name, args, n);

	XtVaGetValues(widget, XmNmaxLength, &max_length, 0);

	fe_SetupPasswdText(widget, max_length);

	return widget;
}

#endif  /* EDITOR */

void
fe_TextFieldSetString(Widget widget, char* value, Boolean notify)
{
	XtCallbackRec buf[32]; /* hope that's enough! */
	XtCallbackList callbacks;
	int i;
	if (notify == FALSE) {
		XtVaGetValues(widget, XmNvalueChangedCallback, &callbacks, 0);
		for (i = 0; callbacks[i].callback != NULL; i++) {
			buf[i] = callbacks[i];
		}
		buf[i].callback = NULL;
		buf[i].closure = NULL;
		
		XtRemoveAllCallbacks(widget, XmNvalueChangedCallback);
	}
	fe_SetTextFieldAndCallBack(widget, value);
	if (notify == FALSE) {
		XtAddCallbacks(widget, XmNvalueChangedCallback, buf);
	}
}

#ifdef EDITOR

static char*
fe_TextFieldGetString(Widget widget)
{
	return fe_GetTextField(widget);
}

static Pixel
fe_get_text_field_background(Widget widget)
{
	return XfeSubResourceGetPixelValue(widget,
									   XtName(widget),
									   XfeClassNameForWidget(widget),
									   XmNbackground,
									   XmCBackground,
									   WhitePixelOfScreen(XtScreen(widget)));
}

void
fe_TextFieldSetEditable(MWContext* context, Widget widget, Boolean editable)
{
	Pixel bg_pixel;
	Arg   args[16];
	Cardinal n;

	/*
	 *    The TextField is so losing, it doesn't set the
	 *    background color to indicate the field is not-editable.
	 *    The Gods of Motif style say thou shalt bestow unto thine
	 *    non-editable TextField thine color of select color-ness.
	 *
	 *    Hmmm this looks butt ugly, maybe have to use a label instead.
	 */
	if (editable) {
		bg_pixel = fe_get_text_field_background(widget);
	} else {
		n = 0;
		XtSetArg(args[n], XmNbackground, &bg_pixel); n++;
		XtGetValues(XtParent(widget), args, n);

#if 0
		/* use select color as background color */
		XmGetColors(XtScreen(widget), fe_cmap(context),
					bg_pixel,
					NULL, /* foreground */
					NULL, /* top shadow */
					NULL, /* bottom shadow */
					&select_pixel);

		bg_pixel = select_pixel;
#endif
	}

	n = 0;
	XtSetArg(args[n], XmNeditable, editable); n++;
	XtSetArg(args[n], XmNcursorPositionVisible, editable); n++;
	XtSetArg(args[n], XmNtraversalOn, editable); n++;
	XtSetArg(args[n], XmNbackground, bg_pixel); n++;
	XtSetValues(widget, args, n);
}

Widget
fe_CreateFrame(Widget parent, char* name,  Arg* p_args, Cardinal p_n)
{
    Widget   frame;
    Widget   title;
    char     namebuf[256];
    Arg      args[16];
    Cardinal n;

    for (n = 0; n < p_n; n++) {
	XtSetArg(args[n], p_args[n].name, p_args[n].value);
    }

    strcpy(namebuf, name);
    strcat(namebuf, "Frame");

    XtSetArg(args[n], XmNshadowType, XmSHADOW_ETCHED_IN); n++;
    frame = XmCreateFrame(parent, namebuf, args, n);
    XtManageChild(frame);

    strcpy(namebuf, name);
    strcat(namebuf, "Title");

    n = 0;
    XtSetArg(args[n], XmNlabelType, XmSTRING); n++;
    XtSetArg(args[n], XmNchildType, XmFRAME_TITLE_CHILD); n++;
    XtSetArg(args[n], XmNchildHorizontalAlignment, XmALIGNMENT_BEGINNING); n++;
    XtSetArg(args[n], XmNchildVerticalAlignment, XmALIGNMENT_CENTER); n++;
    title = XmCreateLabelGadget(frame, namebuf, args, n);
    XtManageChild(title);

    return frame;
}

Widget
fe_CreateFramedForm(Widget parent, char* name, Arg* p_args, Cardinal p_n)
{
    Widget   frame;
    Widget   form;
    Arg      args[16];
    Cardinal n;

    n = 0;
    frame = fe_CreateFrame(parent, name, args, n);
    XtManageChild(frame);

    for (n = 0; n < p_n; n++) {
	XtSetArg(args[n], p_args[n].name, p_args[n].value);
    }

    form = XmCreateForm(frame, name, args, n);

    return form;
}

Widget
fe_CreateFramedRowColumn(Widget parent, char* name, Arg* p_args, Cardinal p_n)
{
    Widget   frame;
    Widget   rowcol;
    Arg      args[16];
    Cardinal n;

    n = 0;
    frame = fe_CreateFrame(parent, name, args, n);
    XtManageChild(frame);

    for (n = 0; n < p_n; n++) {
	XtSetArg(args[n], p_args[n].name, p_args[n].value);
    }
    rowcol = XmCreateRowColumn(frame, name, args, n);

    return rowcol;
}

static Widget
fe_CreateToggleButtonGadget(Widget parent, char* name,
			    Arg* p_args, Cardinal p_nargs)
{
    Arg      args[16];
    Cardinal n = 0;
    Widget   widget;

    for (n = 0; n < p_nargs; n++) {
	XtSetArg(args[n], p_args[n].name, p_args[n].value); n++;
    }

    XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); n++;
    widget = XmCreateToggleButtonGadget(parent, name, args, n);
    return widget;
}


static Widget
fe_CreateRadioButtonGadget(Widget parent, char* name,
			    Arg* p_args, Cardinal p_nargs)
{
    Arg      args[16];
    Cardinal n = 0;
    Widget   widget;

    for (n = 0; n < p_nargs; n++) {
	XtSetArg(args[n], p_args[n].name, p_args[n].value); n++;
    }

    XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
    widget = XmCreateToggleButtonGadget(parent, name, args, n);
    return widget;
}

#endif  /* EDITOR */

Widget
fe_CreatePulldownMenu(Widget parent, char* name, Arg* p_argv, Cardinal p_argc)
{
  unsigned i;
  Widget   popup_menu;
  Arg      argv[8];
  Cardinal argc;
  Visual*  v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  Widget   widget;

  XtCallbackRec* button_callback_rec = NULL;

  for (i = 0; i < p_argc; i++) {
      if (p_argv[i].name == XmNactivateCallback)
	  button_callback_rec = (XtCallbackRec*)p_argv[i].value;
  }

  for (widget = parent; !XtIsWMShell(widget); widget = XtParent(widget))
      ;

  XtVaGetValues(widget, XtNvisual, &v, XtNcolormap, &cmap,
	XtNdepth, &depth, 0);

  argc = 0;
  XtSetArg (argv[argc], XmNvisual, v); argc++;
  XtSetArg (argv[argc], XmNdepth, depth); argc++;
  XtSetArg (argv[argc], XmNcolormap, cmap); argc++;

  popup_menu = XmCreatePulldownMenu(parent, name, argv, argc);

  return popup_menu;
}

#ifdef EDITOR

static Widget
fe_CreateOptionMenuNoLabel(Widget widget, char* name, Arg* args, Cardinal n)
{
	Widget menu = fe_CreateOptionMenu(widget, name, args, n);
	fe_UnmanageChild_safe(XmOptionLabelGadget(menu));
	return menu;
}

/*
 *    This dialog builder stuff was a wishful attempt at doing declarative
 *    dialogs for XFE. I only got as far as using them for the simplest of
 *    dialogs - the Horizontal Rule pref dialog. That's all that uses it,
 *    so it could be ripped and replaced by some simple code for that dialog.
 *    We really should spend the time to do some kind of easier dialog
 *    builder for XFE. One day....djw
 */
typedef struct fe_DialogBuilderCreator {
    char*   name;
    Widget (*func)(Widget parent, char* name, ArgList args, Cardinal argcount);
    Boolean recurse;
} fe_DialogBuilderCreator;

static char XFE_DB_PUSHBUTTONGADGET[] = "XmPushButtonGadget";
static char XFE_DB_LABELGADGET[] = "XmLabelGadget";
static char XFE_DB_FRAME[] = "XmFrame";
static char XFE_DB_FORM[] = "XmForm";
static char XFE_DB_TEXTFIELD[] = "XmTextField";
static char XFE_DB_OPTIONMENU[] = "XmOptionMenu";
static char XFE_DB_PULLDOWNMENU[] = "fe_PulldownMenu";
static char XFE_DB_ROWCOL[] = "XmRowColumn";
static char XFE_DB_FRAMEDFORM[] = "fe_FramedForm";
static char XFE_DB_TOGGLEGADGET[] = "fe_ToggleButtonGadget";
static char XFE_DB_RADIOGADGET[] = "fe_RadioButtonGadget";
static char XFE_DB_FRAMEDROWCOL[] = "fe_FramedRowCol";

static fe_DialogBuilderCreator fe_DialogBuilderDefaultCreators[] = {
    { XFE_DB_PUSHBUTTONGADGET, XmCreatePushButtonGadget, FALSE },
    { XFE_DB_LABELGADGET, XmCreateLabelGadget, FALSE },
    { XFE_DB_TEXTFIELD, fe_CreateTextField, FALSE },
    { XFE_DB_TOGGLEGADGET, fe_CreateToggleButtonGadget, FALSE },
    { XFE_DB_RADIOGADGET, fe_CreateRadioButtonGadget, FALSE },

    { XFE_DB_FRAME, XmCreateFrame, TRUE },
    { XFE_DB_FORM, XmCreateForm, TRUE },
    { XFE_DB_PULLDOWNMENU, fe_CreatePulldownMenu, TRUE },
    { XFE_DB_OPTIONMENU, fe_CreateOptionMenuNoLabel, FALSE },
    { XFE_DB_ROWCOL, XmCreateRowColumn, TRUE },
    { XFE_DB_FRAMEDFORM, fe_CreateFramedForm, TRUE },
    { XFE_DB_FRAMEDROWCOL, fe_CreateFramedRowColumn, TRUE },
    { 0 }
};

static char XFE_DB_END[] = "end";
static char XFE_DB_PUSH[] = "push";
static char XFE_DB_POP[] = "pop";
static char XFE_DB_WIDGETCALLED[] = "widgetcalled";
static char XFE_DB_WIDEST[] = "widest";
static char XFE_DB_TALLEST[] = "tallest";
static char XFE_DB_SETVAL[] = "setval";
static char XFE_DB_SETDATA[] = "setdata";
static char XFE_DB_SETVALCALLED[] = "setvalcalled";
static char XFE_DB_CALLBACK[] = "callback";

typedef struct fe_DialogBuilderArg {
    char*     name;
    XtPointer value;
} fe_DialogBuilderArg;


static fe_DialogBuilderCreator*
find_creator(fe_DialogBuilderCreator* creators, char* name)
{

    int i;
    
    for (i = 0; creators[i].name; i++) {
	if (creators[i].name == name || strcmp(creators[i].name, name) == 0)
	    return &creators[i];
    }
    return NULL;
}

Widget
db_do_work(
	   MWContext* context,
	   Widget parent,
	   fe_DialogBuilderCreator* creators,
	   fe_DialogBuilderArg** instructions_a,
	   void* data)
{
#define MACHINE_NARGS     16
#define MACHINE_NSTACK    16
#define MACHINE_NCALLBACK 16
#define MACHINE_NWIDGETS  32
    Arg           args[MACHINE_NARGS];
    XtPointer     stack[MACHINE_NSTACK];
    XtCallbackRec callbacks[MACHINE_NCALLBACK];
    Cardinal      stackposn;
#define POP()   (stack[--stackposn])
#define PUSH(x) (stack[stackposn++] = (XtPointer)(x))
    Cardinal      nargs;
    Cardinal      ncallbacks;
    Widget        children[MACHINE_NWIDGETS];
    Cardinal      nchildren;
    char*         name;
    XtPointer     value;
    Dimension     width;
    Dimension     max_width;
    Dimension     height;
    Dimension     max_height;
    Widget widget = NULL;
    Widget max_widget;
    int i;
    fe_DialogBuilderCreator* creator;

    nargs = 0;
    stackposn = 0;
    ncallbacks = 0;
    nchildren = 0;

    for (; (*instructions_a)->name != XFE_DB_END; (*instructions_a)++) {
	name = (*instructions_a)->name;
	value = (*instructions_a)->value;

	if (value == (XtPointer)XFE_DB_POP) /* pop is only ever a value */
	    value = POP();

	if (name == XFE_DB_PUSH) {
	    /* push is only ever a name */
	    PUSH(value);
	} else if (name == XFE_DB_WIDGETCALLED) {
	    for (i = 0; i < nchildren; i++) {
		if (strcmp(XtName(children[i]), (char*)value) == 0) {
		    PUSH(children[i]);
		    break;
		}
	    }
	    XP_ASSERT(i < nchildren);
	} else if (name == XFE_DB_WIDEST) {
	    max_width = 0;
	    max_widget = 0;
	    for (i = 0; i < (unsigned)value; i++) {
		widget = (Widget)POP();
		XtVaGetValues(widget, XtNwidth, &width, 0);
		if (width > max_width) {
		    max_width = width;
		    max_widget = widget;
		}
	    }
	    PUSH(max_widget);
	} else if (name == XFE_DB_TALLEST) {
	    max_height = 0;
	    max_widget = 0;
	    for (i = 0; i < (unsigned)value; i++) {
		widget = (Widget)POP();
		XtVaGetValues(widget, XtNheight, &height, 0);
		if (height > max_height) {
		    max_height = height;
		    max_widget = widget;
		}
	    }
	    PUSH(max_widget);
	} else if (name == XFE_DB_SETVAL) {
	    widget = (Widget)POP();
	    XtSetValues(widget, args, nargs);
	    nargs = 0;
	} else if (name == XFE_DB_SETVALCALLED) {
	    for (i = 0; i < nchildren; i++) {
		if (strcmp(XtName(children[i]), (char*)value) == 0) {
		    XtSetValues(children[i], args, nargs);
		    nargs = 0;
		    break;
		}
	    }
	    XP_ASSERT(i < nchildren);
	} else if (name == XFE_DB_SETDATA) {
	    Widget* foo = (Widget*)(((char*) data) + (int) (value));
	    *foo = (Widget)POP();
	} else if (name == XFE_DB_CALLBACK) {
	    callbacks[ncallbacks].callback = (XtCallbackProc)POP();
	    callbacks[ncallbacks].closure = (XtPointer)POP();
	    PUSH(&callbacks[ncallbacks++]);

	    /*
	     *    See if it's a creator.
	     */
	} else if ((creator = find_creator(creators, name))) {

	    widget = (*creator->func)(
				     parent,
				     (char*)value,
				     args,
				     nargs
				     );
	    /*
	     *    Hack, hack, hack for menus which get managed by cascades.
	     */
	    if (creator->name != XFE_DB_PULLDOWNMENU)
			XtManageChild(widget);
	    children[nchildren++] = widget;
	    nargs = 0;
	    /* if (XtIsSubclass(widget, compositeWidgetClass)) { */
	    if (creator->recurse) {
		(*instructions_a)++;
		db_do_work(
			   context,
			   widget,
			   creators,
			   instructions_a,
			   data);
		/* call ourselves */
	    }
	} else { 
	    /*
	     *    Must be an argument.
	     */
	    args[nargs].name = name;
	    args[nargs].value = (XtArgVal)value;
	    nargs++;
	}
    }

#if 0
    XtManageChildren(children, nchildren);
#endif

    return children[0];
}

/*
 *    Stuff for dialogs.
 */
typedef struct fe_HorizontalRulePropertiesDialogData {
	MWContext* context;
    Widget height;
    Widget width;
    Widget width_menu;
    Widget width_units;
    Widget width_pixels;
    Widget width_percent;
    Widget align_left;
    Widget align_center;
    Widget align_right;
    Widget three_d_shading;
} fe_HorizontalRulePropertiesDialogData;

/*
 *    DB program to build Horizontal Rule properties Dialog.
 */
static fe_DialogBuilderArg fe_HorizontalRulePropertiesDialogProgram[] = {

#define OFFSET(x) XtOffset(fe_HorizontalRulePropertiesDialogData *, x)

    { XmNorientation, (XtPointer)XmVERTICAL },
    { XFE_DB_ROWCOL, (XtPointer)"rowcol" },

        { XFE_DB_FRAMEDFORM, "dimensions" },
            { XFE_DB_LABELGADGET, "heightLabel" },
            { XFE_DB_TEXTFIELD, "heightText" },
            { XFE_DB_LABELGADGET, "pixels" },
            { XFE_DB_LABELGADGET, "widthLabel" },
            { XFE_DB_TEXTFIELD, "widthText" },
            { XFE_DB_PULLDOWNMENU, "widthUnitsPulldown" },
                { XFE_DB_PUSHBUTTONGADGET, "percent" },
                { XFE_DB_PUSHBUTTONGADGET, "pixels" },

                { XFE_DB_WIDGETCALLED, "percent" },
             	{ XFE_DB_SETDATA, (XtPointer)OFFSET(width_percent) },
                { XFE_DB_WIDGETCALLED, "pixels" },
             	{ XFE_DB_SETDATA, (XtPointer)OFFSET(width_pixels) },

                { XFE_DB_END, "widthUnitsPulldown" },
            { XFE_DB_WIDGETCALLED, "widthUnitsPulldown" },
            { XmNsubMenuId, XFE_DB_POP },
            { XFE_DB_OPTIONMENU, "widthUnits" },

            /* now do computed attachments, oh boy, we need a compiler */
            { XmNtopAttachment, (XtPointer)XmATTACH_FORM },
            { XmNleftAttachment, (XtPointer)XmATTACH_FORM },
            { XFE_DB_SETVALCALLED, "heightLabel" }, /* set heightLabel */

            { XmNtopAttachment, (XtPointer)XmATTACH_FORM },
            { XmNleftAttachment, (XtPointer)XmATTACH_WIDGET },
            { XFE_DB_WIDGETCALLED, "heightLabel" },
            { XFE_DB_WIDGETCALLED, "widthLabel" },
            { XFE_DB_WIDEST, (XtPointer)2 },
            { XmNleftWidget, XFE_DB_POP },
            { XFE_DB_SETVALCALLED, "heightText" }, /* set heightText */

            { XmNtopAttachment, (XtPointer)XmATTACH_FORM },
            { XmNleftAttachment, (XtPointer)XmATTACH_WIDGET },
            { XFE_DB_WIDGETCALLED, "heightText" },
            { XmNleftWidget, XFE_DB_POP },
            { XFE_DB_SETVALCALLED, "pixels" },     /* set pixel */

            { XmNtopAttachment, (XtPointer)XmATTACH_WIDGET },
            { XFE_DB_WIDGETCALLED, "heightText" },
            { XmNtopWidget, XFE_DB_POP },
            { XmNleftAttachment, (XtPointer)XmATTACH_FORM },
            { XFE_DB_SETVALCALLED, "widthLabel" }, /* set widthLabel */

            { XmNtopAttachment, (XtPointer)XmATTACH_WIDGET },
            { XFE_DB_WIDGETCALLED, "heightText" },
            { XmNtopWidget, XFE_DB_POP },
            { XmNleftAttachment, (XtPointer)XmATTACH_WIDGET },
            { XFE_DB_WIDGETCALLED, "heightLabel" },
            { XFE_DB_WIDGETCALLED, "widthLabel" },
            { XFE_DB_WIDEST, (XtPointer)2 },
            { XmNleftWidget, XFE_DB_POP },
            { XFE_DB_SETVALCALLED, "widthText" }, /* set widthText */

            { XmNtopAttachment, (XtPointer)XmATTACH_WIDGET },
            { XFE_DB_WIDGETCALLED, "heightText" },
            { XmNtopWidget, XFE_DB_POP },
            { XmNleftAttachment, (XtPointer)XmATTACH_WIDGET },
            { XFE_DB_WIDGETCALLED, "widthText" },
            { XmNleftWidget, XFE_DB_POP },
#if 0
            { XmNrightAttachment, (XtPointer)XmATTACH_FORM },
#endif
            { XFE_DB_SETVALCALLED, "widthUnits" }, /* set widthUnits */

         	/* do offsets */
            { XFE_DB_WIDGETCALLED, "widthText" },
         	{ XFE_DB_SETDATA, (XtPointer)OFFSET(width) },
            { XFE_DB_WIDGETCALLED, "heightText" },
         	{ XFE_DB_SETDATA, (XtPointer)OFFSET(height) },
            { XFE_DB_WIDGETCALLED, "widthUnitsPulldown" },
         	{ XFE_DB_SETDATA, (XtPointer)OFFSET(width_menu) },
            { XFE_DB_WIDGETCALLED, "widthUnits" },
         	{ XFE_DB_SETDATA, (XtPointer)OFFSET(width_units) },

            { XFE_DB_END, "dimensions" },

        { XmNorientation, (XtPointer)XmHORIZONTAL },
        { XmNradioBehavior, (XtPointer)TRUE },
        { XFE_DB_FRAMEDROWCOL, "align" },
            { XFE_DB_RADIOGADGET, "left" },
            { XFE_DB_RADIOGADGET, "center" },
            { XFE_DB_RADIOGADGET, "right" },

            { XFE_DB_WIDGETCALLED, "left" },
         	{ XFE_DB_SETDATA, (XtPointer)OFFSET(align_left) },
            { XFE_DB_WIDGETCALLED, "center" },
         	{ XFE_DB_SETDATA, (XtPointer)OFFSET(align_center) },
            { XFE_DB_WIDGETCALLED, "right" },
         	{ XFE_DB_SETDATA, (XtPointer)OFFSET(align_right) },

            { XFE_DB_END, "align" },
        { XFE_DB_TOGGLEGADGET, "threeDShading" },

	    { XFE_DB_WIDGETCALLED, "threeDShading" },
	    { XFE_DB_SETDATA, (XtPointer)OFFSET(three_d_shading) },

        { XFE_DB_END, "rowcol" },

     { XFE_DB_END, "program" }

#undef OFFSET

};

#endif  /* EDITOR */

Widget
fe_CreatePromptDialog(MWContext *context, char* name,
		      Boolean ok, Boolean cancel, Boolean apply, Boolean separator, Boolean modal)
{
  Widget mainw = CONTEXT_WIDGET(context);
  Widget dialog;
  Visual *v = 0;
  Colormap cmap = 0;
  Cardinal depth = 0;
  Arg av [20];
  int ac;

  XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
                 XtNdepth, &depth, 0);
  ac = 0;
  XtSetArg (av[ac], XmNvisual, v); ac++;
  XtSetArg (av[ac], XmNdepth, depth); ac++;
  XtSetArg (av[ac], XmNcolormap, cmap); ac++;
  XtSetArg (av[ac], XmNallowShellResize, TRUE); ac++;
  XtSetArg (av[ac], XmNtransientFor, mainw); ac++;
  if (modal) {
  	XtSetArg (av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
  }
  XtSetArg (av[ac], XmNdialogType, XmDIALOG_QUESTION); ac++;
  XtSetArg (av[ac], XmNdeleteResponse, XmDESTROY); ac++;
  XtSetArg (av[ac], XmNautoUnmanage, False); ac++;
  XtSetArg (av[ac], XmNnoResize, True); ac++;

  dialog = XmCreatePromptDialog (mainw, name, av, ac);

  if (!separator)
	  fe_UnmanageChild_safe(XmSelectionBoxGetChild(dialog,
												   XmDIALOG_SEPARATOR));

  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_TEXT));
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog,
                                                XmDIALOG_SELECTION_LABEL));
  if (!ok)
      fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_OK_BUTTON));
  if (!cancel)
      fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog,
						     XmDIALOG_CANCEL_BUTTON));
#ifdef NO_HELP
  fe_UnmanageChild_safe (XmSelectionBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));
#endif

  if (apply)
      XtManageChild (XmSelectionBoxGetChild (dialog, XmDIALOG_APPLY_BUTTON));

  return dialog;
}

#ifdef EDITOR

int
fe_get_numeric_text_field(Widget widget)
{
	char* p = XmTextFieldGetString(widget); /* numeric so no JIS fix */
	char* q;
	if (p) {
		while (isspace(*p))
			p++;
		if (*p == '\0') return -1;
		for (q = p; *q != '\0'; q++) {
			if (!(isdigit(*q) || isspace(*q)))
				return -1; /* force error */
		}
		return atoi(p);
	} else {
		return -1;
	}
}

static void
fe_editor_hrule_properties_common(MWContext* context,
								  EDT_HorizRuleData* data,
							  fe_HorizontalRulePropertiesDialogData* w_data)
{
    Widget widget;
    
    /* height */
    data->size = fe_get_numeric_text_field(w_data->height);

    /* width */
    XtVaGetValues(w_data->width_menu, XmNmenuHistory, &widget, 0);

    if (widget == w_data->width_pixels)
		data->bWidthPercent = FALSE;
    else
		data->bWidthPercent = TRUE;
    data->iWidth = fe_get_numeric_text_field(w_data->width);

    /* align */
    if (XmToggleButtonGadgetGetState(w_data->align_right) == TRUE)
		data->align = ED_ALIGN_RIGHT;
    else  if (XmToggleButtonGadgetGetState(w_data->align_left) == TRUE)
		data->align = ED_ALIGN_LEFT;
    else
		data->align = ED_ALIGN_CENTER;

    /* shading */
    if (XmToggleButtonGadgetGetState(w_data->three_d_shading) == TRUE)
		data->bNoShade = FALSE;
    else
		data->bNoShade = TRUE;

    data->pExtra = 0;
}

static Boolean
fe_editor_hrule_properties_validate(MWContext* context,
							  fe_HorizontalRulePropertiesDialogData* w_data)
{
	EDT_HorizRuleData  data;
	EDT_HorizRuleData* h = &data;
	unsigned       errors[16];
	unsigned       nerrors = 0;

	fe_editor_hrule_properties_common(context, &data, w_data);
	
	if (RANGE_CHECK(h->size, HRULE_MIN_HEIGHT, HRULE_MAX_HEIGHT))
		errors[nerrors++] = XFE_INVALID_HRULE_HEIGHT;

	if (h->bWidthPercent == TRUE) {
		if (RANGE_CHECK(h->iWidth,
						HRULE_MIN_PERCENT_WIDTH, HRULE_MAX_PERCENT_WIDTH))
			errors[nerrors++] = XFE_INVALID_HRULE_WIDTH;
	} else {
		if (RANGE_CHECK(h->iWidth,
						HRULE_MIN_PIXEL_WIDTH, HRULE_MAX_PIXEL_WIDTH))
			errors[nerrors++] = XFE_INVALID_HRULE_WIDTH;
	}

	if (nerrors > 0) {
		fe_editor_range_error_dialog(context, w_data->width,
									 errors, nerrors);
		return FALSE;
	}

	return TRUE;
}

static void
fe_HorizontalRulePropertiesDialogSet(MWContext* context,
								 fe_HorizontalRulePropertiesDialogData* w_data)
{
    EDT_HorizRuleData e_data;

	EDT_BeginBatchChanges(context);
	fe_editor_hrule_properties_common(context, &e_data, w_data);

    e_data.pExtra = 0;
    
    fe_EditorHorizontalRulePropertiesSet(context, &e_data);
	EDT_EndBatchChanges(context);
}

void
fe_table_percent_label_set(Widget widget, Boolean nested)
{
	char * name = (nested ? "percentOfCell" : "percentOfWindow");

	XmString xm_string = XfeSubResourceGetXmStringValue(widget,
														name,
														XfeClassNameForWidget(widget),
														XmNlabelString,
														XmCXmString,
														NULL);

	if (!xm_string)
	{
		xm_string = XmStringCreateLocalized(name);

		XtVaSetValues(widget, XmNlabelString, xm_string, 0);

		XmStringFree(xm_string);
	}
	/* No need to free xm_string.  The resource converter dtor will */
	else
	{
		XtVaSetValues(widget, XmNlabelString, xm_string, 0);
	}
}

static void
fe_HorizontalRulePropertiesDialogDataGet(
			    MWContext* context,
			    fe_HorizontalRulePropertiesDialogData* w_data
)
{
    EDT_HorizRuleData e_data;
    char buf[16];
    Boolean left;
    Boolean center;
    Boolean right;
    Boolean shading;
	Widget widget;
	Boolean is_nested;

    fe_EditorHorizontalRulePropertiesGet(context, &e_data);
    
    /* height */
    sprintf(buf, "%d", e_data.size);
    fe_SetTextFieldAndCallBack(w_data->height, buf);

    /* width */
    sprintf(buf, "%d", e_data.iWidth);
    fe_SetTextFieldAndCallBack(w_data->width, buf);

    if (e_data.bWidthPercent)
		widget = w_data->width_percent;
	else
		widget = w_data->width_pixels;

	is_nested = EDT_IsInsertPointInTable(context);
	fe_table_percent_label_set(w_data->width_percent, is_nested);

	XtVaSetValues(w_data->width_units, XmNmenuHistory, widget, 0);

    /* align */
    if (e_data.align == ED_ALIGN_RIGHT) {
		left = FALSE;
		center = FALSE;
		right = TRUE;
    } else if (e_data.align == ED_ALIGN_LEFT) {
		left = TRUE;
		center = FALSE;
		right = FALSE;
    } else {
		left = FALSE;
		center = TRUE;
		right = FALSE;
    }
    XmToggleButtonGadgetSetState(w_data->align_right, right, FALSE);
    XmToggleButtonGadgetSetState(w_data->align_left, left, FALSE);
    XmToggleButtonGadgetSetState(w_data->align_center, center, FALSE);

    if (e_data.bNoShade)
		shading = FALSE;
    else
		shading = TRUE;
    XmToggleButtonGadgetSetState(w_data->three_d_shading, shading, FALSE);
}

Widget
fe_EditorHorizontalRulePropertiesCreate(
			      MWContext* context,
			      fe_HorizontalRulePropertiesDialogData* data
			      )
{
  fe_DialogBuilderArg* code;
  Widget dialog;

  /*
   *     Make shell.
   */
  dialog = fe_CreatePromptDialog(context, "horizontalLineProperties",
			       TRUE, TRUE, FALSE, TRUE, TRUE);

  /*
   *   Make the rowcol, because we want to manage it.
   */
  code = fe_HorizontalRulePropertiesDialogProgram;

  db_do_work(context, dialog,
		      fe_DialogBuilderDefaultCreators,
		      &code,
		      data);

  return dialog;
}

static void
fe_hrule_destroy_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
	int* done = (int*)closure;

	*done = XFE_DIALOG_DESTROY_BUTTON;
}

static void
fe_hrule_ok_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
	int* done = (int*)closure;

	*done = XmDIALOG_OK_BUTTON;
}

static void
fe_hrule_apply_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
	int* done = (int*)closure;

	*done = XmDIALOG_APPLY_BUTTON;
}

static void
fe_hrule_cancel_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
	int* done = (int*)closure;

	*done = XmDIALOG_CANCEL_BUTTON;
}

void
fe_EditorHorizontalRulePropertiesDialogDo(MWContext* context)
{
    fe_HorizontalRulePropertiesDialogData widgets;
	int done;
    Widget dialog;

#if 0
    if (!fe_EditorHorizontalRulePropertiesCanDo(context)) {
		XBell(XtDisplay(CONTEXT_WIDGET(context)), 0);
		return;
	}
#endif

	dialog = fe_EditorHorizontalRulePropertiesCreate(context, &widgets);

	/*
	 *   Add a bunch of callbacks to the buttons.
	 */
	XtAddCallback(dialog, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(dialog, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(dialog, XmNdestroyCallback, fe_hrule_destroy_cb, &done);

    /*
     *    Load values.
     */
    fe_HorizontalRulePropertiesDialogDataGet(context, &widgets);
    
    /*
     *    Popup.
     */
    XtManageChild(dialog);

    /*
     *    Wait.
     */
	fe_NukeBackingStore(dialog); /* what does this do? */
	done = XmDIALOG_NONE;
	while (done == XmDIALOG_NONE) {
		fe_EventLoop();

		/*
		 *    Unload data.
		 */
		if (done == XmDIALOG_OK_BUTTON
			&&
			!fe_editor_hrule_properties_validate(context, &widgets)) {
			done = XmDIALOG_NONE;
		}
	}

	if (done == XmDIALOG_OK_BUTTON)
		fe_HorizontalRulePropertiesDialogSet(context, &widgets);

	if (done != XFE_DIALOG_DESTROY_BUTTON)
		XtDestroyWidget(dialog);
}

static void
fe_destroy_cleanup_cb(Widget widget, XtPointer closure, XtPointer cb)
{
    if (closure)
        XtFree(closure);
}

void
fe_DependentListAddDependent(fe_DependentList** list_a,
					  Widget widget, fe_Dependency mask,
					  XtCallbackProc proc, XtPointer closure)
{
	fe_DependentList* list = XtNew(fe_DependentList);

	list->next = *list_a;
	list->widget = widget;
	list->mask = mask;
	list->callback.callback = proc;
	list->callback.closure = closure;

	*list_a = list;
}
		
void
fe_DependentListDestroy(fe_DependentList* list)
{
	fe_DependentList* next;

    for (; list; list = next) {
		next = list->next;
		XtFree((XtPointer)list);
	}
}

typedef struct fe_DependentListCallbackStruct
{
    int     reason;
    XEvent  *event;
	fe_Dependency mask;
	Widget  caller;
	XtPointer callers_data;
} fe_DependentListCallbackStruct;

void
fe_DependentListCallDependents(Widget caller,
							   fe_DependentList* list,
							   fe_Dependency mask,
							   XtPointer callers_data)
{
	fe_DependentListCallbackStruct call_data;

	call_data.reason = 0;
	call_data.event = 0;
	call_data.mask = mask;
	call_data.caller = caller;
	call_data.callers_data = callers_data;

    for (; list; list = list->next) {
		if ((list->mask & mask) != 0)
			(*list->callback.callback)(list->widget, list->callback.closure,
									   &call_data);
	}
}

void
fe_MakeDependent(Widget widget, fe_Dependency mask, 
				 XtCallbackProc proc, XtPointer closure)
{
	MWContext* context = (MWContext *)fe_WidgetToMWContext(widget);

	fe_DependentListAddDependent(
								 &(CONTEXT_DATA(context)->dependents),
								 widget,
								 mask,
								 proc,
								 closure);
}
	
void
fe_CallDependents(MWContext* context, fe_Dependency mask)
{
	fe_DependentListCallDependents(NULL, CONTEXT_DATA(context)->dependents,
								   mask, NULL);
}

struct fe_EditorParagraphPropertiesWidgets;
struct fe_EditorCharacterPropertiesWidgets;
struct fe_EditorLinkPropertiesWidgets;
struct fe_EditorImagePropertiesWidgets;

typedef struct fe_EditorPropertiesWidgets {
	MWContext*                                  context;
	fe_DependentList*                           dependents;
	struct fe_EditorCharacterPropertiesWidgets* character;
	struct fe_EditorLinkPropertiesWidgets*      link;
	struct fe_EditorParagraphPropertiesWidgets* paragraph;
	struct fe_EditorImagePropertiesWidgets*     image;
	fe_Dependency                               changed;
} fe_EditorPropertiesWidgets;

typedef struct fe_EditorParagraphPropertiesWidgets {
	fe_EditorPropertiesWidgets* properties;
	Widget style_menu;
	Widget additional_menu;
	Widget list_style_menu;
	Widget bullet_style_menu;
	Widget start_text;

	Widget  numbering_pulldown;
	Widget  bullet_pulldown;
	TagType paragraph_style;
	TagType additional_style;
	TagType list_style;
	ED_ListType bullet_style;
	ED_Alignment align;
	unsigned start;
} fe_EditorParagraphPropertiesWidgets;

typedef enum fe_JavaScriptModes
{
	JAVASCRIPT_NONE = 0,
	JAVASCRIPT_SERVER,
	JAVASCRIPT_CLIENT
} fe_JavaScriptModes;

typedef struct fe_EditorCharacterPropertiesWidgets {
	fe_EditorPropertiesWidgets* properties;
	Widget form;
	Widget color_default;
	Widget color_custom;
	Widget color_swatch;
	/* something else to hold custom color I guess */

	Widget bold;
	Widget italic;
	Widget fixed;
	Widget strike_thru;
	Widget super_script;
	Widget sub_script;
	Widget blink;
	
	ED_FontSize font_size;
	ED_TextFormat text_attributes;
	ED_TextFormat changed_mask;
	LO_Color      color;
	Boolean is_custom_color;
	fe_JavaScriptModes js_mode;
} fe_EditorCharacterPropertiesWidgets;

typedef char* EDT_LtabbList_t;

typedef struct fe_EditorLinkPropertiesWidgets {
	fe_EditorPropertiesWidgets* properties;
	Widget form;
	Widget displayed_label;
	Widget displayed_text;
	Widget link_text;
	/* others I don't understand */
	Widget target_list;
	EDT_LtabbList_t target_list_data;
	Widget target_current_doc;
	Widget target_selected_file;
	Widget target_label;
	char* selected_filename;
	char* url;
	char* text;
} fe_EditorLinkPropertiesWidgets;

typedef struct fe_EditorImagePropertiesWidgets {
	fe_EditorPropertiesWidgets* properties;
	EDT_ImageData               image_data;
	Widget main_image;
	Widget alt_image;
	Widget alt_text;
	Widget image_height;
	Widget image_width;
	Widget margin_height;
	Widget margin_width;
	Widget margin_solid;
	Widget constrain;
	Boolean  existing_image; /* means there was an image when we loaded */
	Boolean  new_image;      /* means a new image gets loaded */
	Boolean  do_constrain;
	Boolean  do_custom_size;
	Boolean  default_border; /* Don't output a BORDER attribute at all. */
} fe_EditorImagePropertiesWidgets;

#define PROP_CHAR_BOLD    (0x1<<0)
#define PROP_CHAR_ITALIC  (0x1<<1)
#define PROP_CHAR_FIXED   (0x1<<2)
#define PROP_CHAR_SUPER   (0x1<<3)
#define PROP_CHAR_SUB     (0x1<<4)
#define PROP_CHAR_STRIKE  (0x1<<5)
#define PROP_CHAR_BLINK   (0x1<<6)
#define PROP_CHAR_COLOR   (0x1<<7)
#define PROP_CHAR_SIZE    (0x1<<8)

#define PROP_CHAR_SERVER    (0x1<<10)
#define PROP_CHAR_CLIENT    (0x1<<11)
#define PROP_CHAR_UNDERLINE (0x1<<12)

#define PROP_PARA_STYLE   (0x1<<13)
#define PROP_PARA_LIST    (0x1<<14)
#define PROP_PARA_BULLET  (0x1<<15)
#define PROP_PARA_ALIGN   (0x1<<16)
#define PROP_LINK_TEXT    (0x1<<17)
#define PROP_LINK_HREF    (0x1<<18)
#define PROP_IMAGE_MAIN_IMAGE (0x1<<19)
#define PROP_IMAGE_ALT_IMAGE  (0x1<<20)
#define PROP_IMAGE_ALT_TEXT   (0x1<<21)
#define PROP_IMAGE_ALIGN      (0x1<<22)
#define PROP_IMAGE_HEIGHT     (0x1<<23)
#define PROP_IMAGE_WIDTH      (0x1<<24)
#define PROP_IMAGE_MARGIN_HEIGHT (0x1<<25)
#define PROP_IMAGE_MARGIN_WIDTH  (0x1<<26)
#define PROP_IMAGE_MARGIN_BORDER (0x1<<27)
#define PROP_IMAGE_COPY       (0x1<<28)
#define PROP_IMAGE_IMAP       (0x1<<29)
#define PROP_LINK_LIST    (0x1<<30)

#define PROP_CHAR_STYLE   \
(PROP_CHAR_BOLD|PROP_CHAR_ITALIC|PROP_CHAR_UNDERLINE|PROP_CHAR_FIXED| \
 PROP_CHAR_STRIKE|PROP_CHAR_SUPER|PROP_CHAR_SUB|PROP_CHAR_BLINK)
#define PROP_CHAR_JAVASCRIPT (PROP_CHAR_CLIENT|PROP_CHAR_SERVER)
#define PROP_CHAR_ALL     \
(PROP_CHAR_STYLE|PROP_CHAR_COLOR|PROP_CHAR_SIZE|PROP_CHAR_JAVASCRIPT)
#define PROP_PARA_ALL     \
(PROP_PARA_STYLE|PROP_PARA_LIST|PROP_PARA_BULLET|PROP_PARA_ALIGN)
#define PROP_LINK_ALL     \
(PROP_LINK_TEXT|PROP_LINK_HREF|PROP_LINK_LIST)
#define PROP_IMAGE_DIMENSIONS (PROP_IMAGE_HEIGHT|PROP_IMAGE_WIDTH)
#define PROP_IMAGE_SPACE      \
(PROP_IMAGE_MARGIN_HEIGHT|PROP_IMAGE_MARGIN_WIDTH|PROP_IMAGE_MARGIN_BORDER)
#define PROP_IMAGE_ALL    \
(PROP_IMAGE_MAIN_IMAGE|PROP_IMAGE_ALT_IMAGE|PROP_IMAGE_ALT_TEXT|  \
 PROP_IMAGE_ALIGN|PROP_IMAGE_DIMENSIONS|PROP_IMAGE_SPACE|         \
 PROP_IMAGE_COPY|PROP_IMAGE_IMAP)

static void
fe_update_dependents(Widget caller,
					 fe_EditorPropertiesWidgets* p_data, fe_Dependency mask)
{
	p_data->changed |= mask;
	fe_DependentListCallDependents(caller, p_data->dependents, mask, NULL);
}

static void
fe_register_dependent(fe_EditorPropertiesWidgets* p_data, Widget widget,
					  fe_Dependency mask, XtCallbackProc proc, XtPointer closure)
{
	fe_DependentListAddDependent(&p_data->dependents,
								 widget,
								 mask,
								 proc,
								 closure);
}

typedef struct fe_style_data {
	char* name;
	unsigned data;
} fe_style_data;

static struct fe_style_data fe_paragraph_style[] = {
    { "normal",          P_NSDT       },
    { "headingOne",      P_HEADER_1   },
    { "headingTwo",      P_HEADER_2   },
    { "headingThree",    P_HEADER_3   },
    { "headingFour",     P_HEADER_4   },
    { "headingFive",     P_HEADER_5   },
    { "headingSix",      P_HEADER_6   },
    { "address",         P_ADDRESS    },
    { "formatted",       P_PREFORMAT  },
    { "listItem",        P_LIST_ITEM  },
    { "descriptionItem", P_DESC_TITLE },
    { "descriptionText", P_DESC_TEXT  },
    { 0 }
};

static struct fe_style_data fe_additional_style[] = {
    { "default",         P_NSDT       },
    { "list",            P_LIST_ITEM  },
    { "blockQuote",      P_BLOCKQUOTE },
    { 0 }
};

static struct fe_style_data fe_list_style[] = {
    { "unnumbered",      P_UNUM_LIST  },
    { "numbered",        P_NUM_LIST   },
    { "directory",       P_DIRECTORY  },
    { "menu",            P_MENU       },
    { "description",     P_DESC_LIST  },
    { 0 }
};

static struct fe_style_data fe_bullet_style[] = {
    { "automatic",       ED_LIST_TYPE_DEFAULT },
    { "solidCircle",     ED_LIST_TYPE_DISC    },
    { "openSquare",      ED_LIST_TYPE_SQUARE  },
    { "openCircle",      ED_LIST_TYPE_CIRCLE  },
    { 0 }
};

static struct fe_style_data fe_numbering_style[] = {
    { "automatic",       ED_LIST_TYPE_DEFAULT        },
    { "digital",         ED_LIST_TYPE_DIGIT          },
    { "upperRoman",      ED_LIST_TYPE_BIG_ROMAN      },
    { "lowerRoman",      ED_LIST_TYPE_SMALL_ROMAN    },
    { "upperAlpha",      ED_LIST_TYPE_BIG_LETTERS    },
    { "lowerAlpha",      ED_LIST_TYPE_SMALL_LETTERS  },
    { 0 }
};

static struct fe_style_data fe_align_style[] = {
    { "left",       ED_ALIGN_LEFT   },
    { "center",     ED_ALIGN_ABSCENTER },
    { "right",      ED_ALIGN_RIGHT  },
    { 0 }
};

static unsigned
fe_convert_value_to_index(fe_style_data* style_data, unsigned value)
{
	unsigned i;

	for (i = 0; style_data[i].name; i++) {
		if (style_data[i].data == value)
			return i;
	}
	/* maybe should assert */
	return 0;
}

static void
fe_paragraph_style_menu_update_cb(Widget widget, XtPointer closure,
								  XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data = (fe_EditorParagraphPropertiesWidgets*)closure;

	TagType  type = w_data->paragraph_style;
	unsigned index = fe_convert_value_to_index(fe_paragraph_style, type);

	fe_OptionMenuSetHistory(widget, index);
}

static void
fe_paragraph_style_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data
		= (fe_EditorParagraphPropertiesWidgets*)closure;
	unsigned foo = (unsigned)fe_GetUserData(widget);
	TagType  type = (TagType)foo;

	w_data->paragraph_style = type;
	if (type == P_LIST_ITEM) {
		w_data->additional_style = P_LIST_ITEM;
	} else if (w_data->additional_style == P_LIST_ITEM) {
		w_data->additional_style = P_NSDT; /* default */
	}

	fe_update_dependents(widget, w_data->properties, PROP_PARA_STYLE|PROP_PARA_LIST);
}

static void
fe_additional_style_menu_update_cb(Widget widget, XtPointer closure,
								  XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data = (fe_EditorParagraphPropertiesWidgets*)closure;

	TagType  type = w_data->additional_style;
	unsigned index = fe_convert_value_to_index(fe_additional_style, type);

	fe_OptionMenuSetHistory(widget, index);
}

static void
fe_additional_style_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data
		= (fe_EditorParagraphPropertiesWidgets*)closure;
	unsigned foo = (unsigned)fe_GetUserData(widget);
	TagType  type = (TagType)foo;

	w_data->additional_style = type;
	if (type == P_LIST_ITEM)
		w_data->paragraph_style = P_LIST_ITEM;

	fe_update_dependents(widget, w_data->properties, PROP_PARA_STYLE|PROP_PARA_LIST);
}

static void
fe_list_style_menu_update_cb(Widget widget, XtPointer closure,
								  XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data = (fe_EditorParagraphPropertiesWidgets*)closure;

	TagType  type = w_data->additional_style;
	Boolean  enabled = FALSE;
	unsigned index;

	if (type == P_LIST_ITEM) {
		type = w_data->list_style;
		index = fe_convert_value_to_index(fe_list_style, type);
		enabled = TRUE;

		fe_OptionMenuSetHistory(widget, index);
	}

	XtVaSetValues(XmOptionButtonGadget(widget), XmNsensitive, enabled, 0);
}

static void
fe_list_style_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data
		= (fe_EditorParagraphPropertiesWidgets*)closure;
	unsigned foo = (unsigned)fe_GetUserData(widget);
	TagType  type = (TagType)foo;

	w_data->list_style = type;
	
	fe_update_dependents(widget, w_data->properties, PROP_PARA_LIST|PROP_PARA_BULLET);
}

static void
fe_bullet_style_menu_update_cb(Widget widget, XtPointer closure,
								  XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data = (fe_EditorParagraphPropertiesWidgets*)closure;

	TagType  type = w_data->additional_style;
	Boolean  enabled = FALSE;
	unsigned index = 0;         /* keep -O happy */
	Widget   cascade = XmOptionButtonGadget(widget);
	Widget   pulldown = NULL;   /* keep -O happy */

	if (type == P_LIST_ITEM) {
		type = w_data->list_style;
		if (type == P_NUM_LIST) {
			enabled = TRUE;
			index = fe_convert_value_to_index(fe_numbering_style,
											  w_data->bullet_style);
			pulldown = w_data->numbering_pulldown;
		} else if (type == P_UNUM_LIST) {
			enabled = TRUE;
			index = fe_convert_value_to_index(fe_bullet_style,
											  w_data->bullet_style);
			pulldown = w_data->bullet_pulldown;
		}

		if (enabled) {
			XtVaSetValues(cascade, XmNsubMenuId, pulldown, 0);
			fe_OptionMenuSetHistory(widget, index);
		}
	}

	XtVaSetValues(cascade, XmNsensitive, enabled, 0);
}

static void
fe_bullet_style_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data = (fe_EditorParagraphPropertiesWidgets*)closure;

	ED_ListType	type = (ED_ListType)fe_GetUserData(widget);

	w_data->bullet_style = type;

	fe_update_dependents(widget, w_data->properties, PROP_PARA_BULLET);
}

static Widget
fe_create_style_pulldown(Widget parent, char* name, fe_style_data* style_data,
					 Arg* p_args, Cardinal p_n)
{
	Cardinal nchildren;
	Widget   children[16];
	Widget   pulldown;
	Arg      args[8];
	Cardinal n;
	Cardinal m;
	XtCallbackRec* button_callback_rec = 0;
	XtCallbackRec* update_callback_rec = 0;

	for (n = m = 0; n < p_n; n++) {
		if (p_args[n].name == XmNactivateCallback)
			button_callback_rec = (XtCallbackRec*)p_args[n].value;
		else if (p_args[n].name == XmNvalueChangedCallback)
			update_callback_rec = (XtCallbackRec*)p_args[n].value;
		else {
			XtSetArg(args[m], p_args[n].name, p_args[n].value); m++;
		}
	}

	n = m;
	pulldown = fe_CreatePulldownMenu(parent, name, args, n);

	for (nchildren = 0; style_data[nchildren].name; nchildren++) {

		n = 0;
		XtSetArg(args[n], XmNuserData, style_data[nchildren].data); n++;
		children[nchildren] = XmCreatePushButtonGadget(
													   pulldown,
													   style_data[nchildren].name,
													   args,
													   n
													   );
		if (button_callback_rec) {
			XtAddCallback(children[nchildren],
						  XmNactivateCallback,
						  button_callback_rec->callback,
						  (XtPointer)button_callback_rec->closure);
		}
	}
	XtManageChildren(children, nchildren);
	
	return pulldown;
}

static Widget
fe_create_style_menu(Widget parent, char* name, fe_style_data* style_data,
					 Arg* p_args, Cardinal p_n)
{
	Widget   pulldown;
	Widget   option;
	Arg      args[8];
	Arg      rc_args[8];
	Cardinal n;
	Cardinal my_n;
	Cardinal rc_n;

	for (my_n = rc_n = n = 0; n < p_n; n++) {
		if (
			p_args[n].name == XmNactivateCallback
			||
			p_args[n].name == XmNvalueChangedCallback
		) {
			XtSetArg(rc_args[rc_n], p_args[n].name, p_args[n].value); rc_n++;
		} else {
			XtSetArg(args[my_n], p_args[n].name, p_args[n].value); my_n++;
		}
	}

	pulldown = fe_create_style_pulldown(parent, name, style_data, rc_args, rc_n);

	XtSetArg(args[my_n], XmNsubMenuId, pulldown); my_n++;
	option = fe_CreateOptionMenu(parent,	name, args, my_n);
	fe_UnmanageChild_safe(XmOptionLabelGadget(option));

	return option;
}

static void
fe_paragraph_align_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data =
		(fe_EditorParagraphPropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;

	/*
	 *    We get a callback on both the unset of the old toggle,
	 *    and the set of the new, so ignore the former, it's not
	 *    interesting.
	 */
	if (info->set) {

		w_data->align = (ED_Alignment)fe_GetUserData(widget);	

		fe_update_dependents(widget, w_data->properties, PROP_PARA_ALIGN);
	}
}

static void
fe_paragraph_align_update_cb(Widget widget,
							 XtPointer closure, XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data =
		(fe_EditorParagraphPropertiesWidgets*)closure;
	ED_Alignment align = (ED_Alignment)fe_GetUserData(widget);
	ED_Alignment w_align = w_data->align;

	if (w_align == ED_ALIGN_DEFAULT)
	    w_align = ED_ALIGN_LEFT;

	if (w_align == ED_ALIGN_CENTER) /* just in case the BE changes */
	    w_align = ED_ALIGN_ABSCENTER;

    XmToggleButtonGadgetSetState(widget, (w_align == align), FALSE);
}

static void
fe_set_text_field(Widget widget, char* value,
				  XtCallbackProc cb, XtPointer closure)
{
	if (cb)
		XtRemoveCallback(widget, XmNvalueChangedCallback,
						 cb, closure);

	if (!value)
		value = "";

	fe_SetTextFieldAndCallBack(widget, value);
	
	if (cb)
		XtAddCallback(widget, XmNvalueChangedCallback,
					  cb, closure);
}

void
fe_set_numeric_text_field(Widget widget, unsigned value)
{
	char buf[32];

	sprintf(buf, "%d", value);

	fe_TextFieldSetString(widget, buf, FALSE);
}

static void
fe_paragraph_start_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data =
		(fe_EditorParagraphPropertiesWidgets*)closure;

	fe_update_dependents(widget, w_data->properties, PROP_PARA_BULLET);
}

#if 0
static unsigned
fe_aztoui(char* s, Boolean upper)
{
    char* p;
	unsigned rv = 0;
	unsigned high;
	unsigned low;
	if (upper) {
	  low = 'A';
	  high = 'Z';
	} else {
	  low = 'a';
	  high = 'z';
	}
	for (p = s; *p >= low && *p <= high; p++) {
	    rv *= 26;
		rv += (*p - low) + 1;
	}
	return rv;
} 

static int
fe_uitoaz(char* buf, unsigned value, Boolean upper)
{
    char* p;
	unsigned base = 26;
	unsigned low;
	if (upper) {
	  low = 'A';
	} else {
	  low = 'a';
	}

	if (value == 0) {
		buf[0] = '\0';
		return 0;
	}
	value--;

	for (p = buf; base < value; p++)
	    base = base * 26;

	p[1] = '\0';


	for (; p >= buf; p--) {
	    *p = (value % 26) + low;
		value /= 26;
	}
	return strlen(buf);
}
#endif

static void
fe_paragraph_start_update_cb(Widget widget,
							 XtPointer closure, XtPointer call_data)
{
	fe_EditorParagraphPropertiesWidgets* w_data =
		(fe_EditorParagraphPropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;
	char buf[256];
	Boolean enabled;

	if (info->caller == widget)
		return; /* don't call ourselves */

	if (w_data->paragraph_style == P_LIST_ITEM
		&&
		w_data->list_style==P_NUM_LIST) {
#if 0
		if (
			w_data->bullet_style == ED_LIST_TYPE_DIGIT
			||
			w_data->bullet_style == ED_LIST_TYPE_BIG_ROMAN
			||
			w_data->bullet_style == ED_LIST_TYPE_SMALL_ROMAN
		) {
			sprintf(buf, "%d", w_data->start);
		} else if (w_data->bullet_style == ED_LIST_TYPE_BIG_LETTERS) {
		    fe_uitoaz(buf, w_data->start, TRUE);
		} else  if (w_data->bullet_style == ED_LIST_TYPE_SMALL_LETTERS) {
		    fe_uitoaz(buf, w_data->start, FALSE);
		}
#else
		if (w_data->start < 1)
			w_data->start = 1;
		sprintf(buf, "%d", w_data->start);
#endif
		enabled = TRUE;
	} else {
		buf[0] = '\0';
		enabled = FALSE;
	}

	fe_TextFieldSetEditable(w_data->properties->context, widget, enabled);
	fe_set_text_field(widget, buf, fe_paragraph_start_cb, (XtPointer)w_data);
}

static void
fe_EditorParagraphPropertiesDialogDataGet(
								  MWContext* context,
								  fe_EditorParagraphPropertiesWidgets* w_data)
{
	EDT_ListData list_data;

	fe_EditorParagraphPropertiesGetAll(context,
									   &w_data->paragraph_style,
									   &list_data,
									   &w_data->align);
	
	switch (list_data.iTagType) {
	case P_BLOCKQUOTE: /* block quotes */
		w_data->additional_style = P_BLOCKQUOTE;
		w_data->list_style = P_UNUM_LIST;
		w_data->bullet_style = ED_LIST_TYPE_DEFAULT;
		break;

	case P_NUM_LIST: /* lists */
		w_data->start = list_data.iStart;
		if (w_data->start < 1)
			w_data->start = 1;
		/*FALLTHRU*/
	case P_UNUM_LIST:
	case P_DIRECTORY:
	case P_MENU:
	case P_DESC_LIST:
		w_data->additional_style = P_LIST_ITEM;
		w_data->list_style = list_data.iTagType;
		w_data->bullet_style = list_data.eType;
		break;

	default:
		w_data->additional_style = P_NSDT;
		w_data->list_style = P_UNUM_LIST;
		w_data->bullet_style = ED_LIST_TYPE_DEFAULT;
		break;
	}

	/*
	 *    Update the display.
	 */
	fe_update_dependents(NULL, w_data->properties, PROP_PARA_ALL);
}

static void
fe_EditorParagraphPropertiesDialogSet(
								 MWContext* context,
								 fe_EditorParagraphPropertiesWidgets* w_data)
{
	EDT_ListData list_data;
	char* p = NULL;

	memset(&list_data, 0, sizeof(EDT_ListData));

	if (w_data->paragraph_style == P_LIST_ITEM) { /* do list stuff */
		list_data.iTagType = w_data->list_style;
		list_data.bCompact = 0; /* ??? */
		list_data.eType = w_data->bullet_style;
		p = fe_TextFieldGetString(w_data->start_text);
		list_data.iStart = atoi(p);
		if (p)
			XtFree(p);

		fe_EditorParagraphPropertiesSetAll(context,
										   w_data->paragraph_style,
										   &list_data,
										   w_data->align);
		
	} else if (w_data->additional_style == P_BLOCKQUOTE) { /* do quote */
		
		list_data.iTagType = P_BLOCKQUOTE;
		list_data.bCompact = 0; /* ??? */
		list_data.eType = ED_LIST_TYPE_DEFAULT;

		fe_EditorParagraphPropertiesSetAll(context,
										   w_data->paragraph_style,
										   &list_data,
										   w_data->align);
	} else {
		fe_EditorParagraphPropertiesSetAll(context,
										   w_data->paragraph_style,
										   NULL,
										   w_data->align);
	}
	

}

static void
fe_make_paragraph_page(
					   MWContext *context,
					   Widget parent,
					   fe_EditorParagraphPropertiesWidgets* w_data)
{
	Arg args[16];
	Cardinal n;
	Widget form;
	Widget paragraph_label;
	Widget paragraph_option;
	Widget additional_label;
	Widget additional_option;
	Widget list_frame;
	Widget list_form;
	Widget list_style_label;
	Widget list_style_option;
	Widget bullet_style_option;
	Widget starting_text;
	Widget starting_label;
	Widget align_frame;
	Widget align_rc;
	Widget children[16];
	Cardinal nchildren;
	XtCallbackRec callback;

#if 0
	n = 0;
	form = XmCreateForm(parent, "paragraphProperties", args, n);
	XtManageChild(form);
#else
	form = parent;
#endif

	/*
	 *    Paragraph Style.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	paragraph_label = XmCreateLabelGadget(form, "styleLabel", args, n);
	XtManageChild(paragraph_label);

	n = 0;
	callback.callback = fe_paragraph_style_cb;
	callback.closure = (XtPointer)w_data;
	XtSetArg(args[n], XmNactivateCallback, &callback); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, paragraph_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	paragraph_option = fe_create_style_menu(form, "style", fe_paragraph_style,
											args, n);
	XtManageChild(paragraph_option);

	fe_register_dependent(w_data->properties, paragraph_option,
						  FE_MAKE_DEPENDENCY(PROP_PARA_STYLE|PROP_PARA_LIST),
						  fe_paragraph_style_menu_update_cb, (XtPointer)w_data);

	/*
	 *    Additional Style.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, paragraph_option); n++;
	additional_label = XmCreateLabelGadget(form, "additionalLabel", args, n);
	XtManageChild(additional_label);

	n = 0;
	callback.callback = fe_additional_style_cb;
	callback.closure = (XtPointer)w_data;
	XtSetArg(args[n], XmNactivateCallback, &callback); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, paragraph_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, paragraph_option); n++;
	additional_option = fe_create_style_menu(form, "additional", fe_additional_style,
											args, n);
	XtManageChild(additional_option);

	fe_register_dependent(w_data->properties, additional_option,
						  FE_MAKE_DEPENDENCY(PROP_PARA_STYLE|PROP_PARA_LIST),
						  fe_additional_style_menu_update_cb, (XtPointer)w_data);
	/*
	 *    List frame and friends. THIS IS SO BORING!!!!!!!!!!!!!
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, paragraph_option); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	list_frame = fe_CreateFrame(form, "list", args, n);
	XtManageChild(list_frame);
	
	n = 0;
	list_form = XmCreateForm(list_frame, "list", args, n);
	XtManageChild(list_form);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	list_style_label = XmCreateLabelGadget(list_form, "listLabel", args, n);
	XtManageChild(list_style_label);

	n = 0;
	callback.callback = fe_list_style_cb;
	callback.closure = (XtPointer)w_data;
	XtSetArg(args[n], XmNactivateCallback, &callback); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, list_style_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	list_style_option = fe_create_style_menu(list_form, "listStyle", fe_list_style,
											args, n);
	XtManageChild(list_style_option);

	fe_register_dependent(w_data->properties, list_style_option,
						  FE_MAKE_DEPENDENCY(PROP_PARA_LIST),
						  fe_list_style_menu_update_cb, (XtPointer)w_data);

	n = 0;
	callback.callback = fe_bullet_style_cb;
	callback.closure = (XtPointer)w_data;
	XtSetArg(args[n], XmNactivateCallback, &callback); n++;
	w_data->numbering_pulldown = fe_create_style_pulldown(list_form,
														  "bulletStyle",
														  fe_numbering_style,
														  args, n);
	w_data->bullet_pulldown = fe_create_style_pulldown(list_form,
													   "bulletStyle",
													   fe_bullet_style,
													   args, n);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, list_style_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, list_style_option); n++;
	XtSetArg(args[n], XmNsubMenuId, w_data->bullet_pulldown); n++;
	bullet_style_option = fe_CreateOptionMenu(list_form, "bulletStyle", args, n);
	fe_UnmanageChild_safe(XmOptionLabelGadget(bullet_style_option));
	XtManageChild(bullet_style_option);

	fe_register_dependent(w_data->properties, bullet_style_option,
						  FE_MAKE_DEPENDENCY(PROP_PARA_BULLET|PROP_PARA_LIST),
						  fe_bullet_style_menu_update_cb, (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, bullet_style_option); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, bullet_style_option); n++;
	w_data->start_text = starting_text = 
		fe_CreateTextField(list_form, "startText", args, n);
	XtManageChild(starting_text);

	XtAddCallback(starting_text, XmNvalueChangedCallback,
				  fe_paragraph_start_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, starting_text,
						  FE_MAKE_DEPENDENCY(PROP_PARA_BULLET|PROP_PARA_LIST),
						  fe_paragraph_start_update_cb, (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, bullet_style_option); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, starting_text); n++;
	starting_label = XmCreateLabelGadget(list_form, "startLabel", args, n);
	XtManageChild(starting_label);

	/*
	 *    Align.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, list_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	align_frame = fe_CreateFrame(form, "align", args, n);
	XtManageChild(align_frame);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNradioBehavior, TRUE); n++;
	align_rc = XmCreateRowColumn(align_frame, "align", args, n);
	XtManageChild(align_rc);

	for (nchildren = 0; fe_align_style[nchildren].name; nchildren++) {
		n = 0;
		XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
		XtSetArg(args[n], XmNuserData, fe_align_style[nchildren].data); n++;
		children[nchildren] = XmCreateToggleButtonGadget(
											 align_rc,
											 fe_align_style[nchildren].name,
											 args, n);

		XtAddCallback(children[nchildren], XmNvalueChangedCallback,
					  fe_paragraph_align_cb, (XtPointer)w_data);
		fe_register_dependent(w_data->properties,
							 children[nchildren], 
							 FE_MAKE_DEPENDENCY(PROP_PARA_ALIGN),
							 fe_paragraph_align_update_cb, (XtPointer)w_data);
	}

	XtManageChildren(children, nchildren);

#if 0
	/* Humour for Beta */
	/* Apparently our testers have no humour - oh well */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, align_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	align_rc = XmCreateLabelGadget(form, "spaceAvailable", args, n);
	XtManageChild(align_rc);
#endif
}

static void
fe_link_text_changed_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorLinkPropertiesWidgets* w_data =
		(fe_EditorLinkPropertiesWidgets*)closure;
	
	fe_update_dependents(widget, w_data->properties, PROP_LINK_TEXT);
}

static char*
fe_image_get_name(fe_EditorImagePropertiesWidgets* w_data)
{
	char* p = NULL;

	if (w_data->main_image)
		p = fe_TextFieldGetString(w_data->main_image);
	
	if (!p && w_data->alt_image)
		p = fe_TextFieldGetString(w_data->alt_image);

	if (!p && w_data->alt_text)
		p = fe_TextFieldGetString(w_data->alt_text);

	return p;
}

static void
fe_link_text_update_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorLinkPropertiesWidgets* w_data =
		(fe_EditorLinkPropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;
	Boolean enabled = FALSE;
	char*   text;
	char* free_text = NULL;
	char* label_string;
	int   id;
	XmString xm_string;

	if (info->caller == widget)
		return; /* don't call ourselves */

	/*
	 *    If we have an image, use the image name as the current
	 *    displayed text.
	 */
	if (w_data->properties->image != NULL) {
		free_text = text = fe_image_get_name(w_data->properties->image);
		enabled = FALSE;
		id = XFE_EDITOR_LINK_TEXT_LABEL_IMAGE;
		/* Linked image: */
	} else if (w_data->text) {
		text = w_data->text;
		enabled = FALSE;
		id = XFE_EDITOR_LINK_TEXT_LABEL_TEXT;
		/* Linked text: */
	} else {
		text = "";
		enabled = TRUE;
		id = XFE_EDITOR_LINK_TEXT_LABEL_NEW;
		/* Enter the text of the link: */
	}

	fe_set_text_field(widget, text, fe_link_text_changed_cb,(XtPointer)w_data);
	fe_TextFieldSetEditable(w_data->properties->context, widget, enabled);
	
	/*
	 *    While we are here set the label also.
	 */
	label_string = XP_GetString(id);
	xm_string = XmStringCreateLocalized(label_string);
	XtVaSetValues(w_data->displayed_label, XmNlabelString, xm_string, 0);
	XmStringFree(xm_string);

	if (free_text)
		XtFree(free_text);
}

static void
fe_link_href_changed_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	
	fe_EditorLinkPropertiesWidgets* w_data =
		(fe_EditorLinkPropertiesWidgets*)closure;

	fe_update_dependents(widget, w_data->properties, PROP_LINK_HREF);
}

static void
fe_link_href_update_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorLinkPropertiesWidgets* w_data =
		(fe_EditorLinkPropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;
	char*   text;

	if (info->caller == widget)
		return; /* don't call ourselves */

	/*
	 *    If we have an image, use the image name as the current
	 *    displayed text.
	 */
	if (w_data->url) {
		text = w_data->url;
	} else {
		text = "";
	}

	fe_set_text_field(widget, text, fe_link_href_changed_cb,(XtPointer)w_data);
	w_data->url = NULL;
}

static char*
fe_basename(char* target, char* source)
{
	char* p;
			
	p = strrchr(source, '/'); /* find last slash */

	if (p) { /* should be */
		strcpy(target, p+1);
	} else {
		strcpy(target, source);
	}
	return target;
}

static char*
fe_dirname(char* target, char* source)
{
	char* p;
			
	p = strrchr(source, '/'); /* find last slash */

	if (p) { /* should be */
		if (p == source) {
			strcpy(target, "/");
		} else {
			memcpy(target, source, p - source);
			target[p - source] = '\0';
		}
	} else {
		strcpy(target, ".");
	}
	return target;
}

static Boolean
fe_file_has_same_directory(MWContext* context, char* pathname)
{
	char my_dirname[MAXPATHLEN];
	char your_dirname[MAXPATHLEN];

	fe_EditorMakeName(context, your_dirname, sizeof(your_dirname));
	fe_dirname(my_dirname, your_dirname);
	fe_dirname(your_dirname, pathname);

	return (strcmp(my_dirname, your_dirname) == 0);
}

static void
fe_link_properties_list_update(fe_EditorLinkPropertiesWidgets*, Boolean);

static void
fe_editor_browse_file_of_text(MWContext* context, Widget text_field)
{
    char* before_save;
	char* before_changed = NULL;
	char* after;
	char* p;

	/*
	 *    Strip #target from the browse default filename.
	 */
	before_save = fe_TextFieldGetString(text_field);

	/*
	 *    If the text field is a http: form URL, then zero it,
	 *    because we are not browsing for a file.
	 */
	if (before_save != NULL) {
		fe_StringTrim(before_save);

		if ((p = strchr(before_save, ':')) != NULL) {
			if (NET_IsLocalFileURL(before_save)) {
				before_changed = XtNewString(&p[1]);
			} else {
				before_changed = XtNewString("");
			}
		} else if ((p = strchr(before_save, '#')) != NULL) {
			*p = '\0';
			before_changed = XtNewString(before_save);
			*p = '#';
		}
		if (before_changed != NULL)
			fe_TextFieldSetString(text_field, before_changed, FALSE);
	}
	
	fe_browse_file_of_text(context, text_field, FALSE);

	after = fe_TextFieldGetString(text_field);
	fe_StringTrim(after);

	/*
	 *    If we zapped it, and there was no change by the user,
	 *    put it back the way it was.
	 */
	if (before_changed != NULL && strcmp(after, before_changed) == 0) {
		fe_TextFieldSetString(text_field, before_save, FALSE);
	}
	/*
	 *    Or, if the main file, and the link file are in the same
	 *    directory, simplfy the name.
	 */
	else if (fe_file_has_same_directory(context, after)) {
		char  basename[MAXPATHLEN];
		fe_basename(basename, after);
		fe_TextFieldSetString(text_field, basename, FALSE);
	}

	if (before_save)
		XtFree(before_save);
	if (before_changed)
		XtFree(before_changed);
	if (after)
		XtFree(after);
}

static void
fe_link_href_browse_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorLinkPropertiesWidgets* w_data =
		(fe_EditorLinkPropertiesWidgets*)closure;
	MWContext* context = w_data->properties->context;
	Widget text_field = w_data->link_text;

	fe_editor_browse_file_of_text(context, text_field);

	fe_update_dependents(widget, w_data->properties, PROP_LINK_LIST);

	/*
	 *    Set the toggle to selected file, and update.
	 */
	XmToggleButtonGadgetSetState(w_data->target_current_doc, FALSE, FALSE);
	XmToggleButtonGadgetSetState(w_data->target_selected_file, TRUE, FALSE);

	fe_link_properties_list_update(w_data, FALSE);
}

static void
fe_browse_to_text_field_cb(Widget widget,
						   XtPointer closure, XtPointer call_data)
{
    MWContext* context = (MWContext*)closure;
	Widget     text_field = (Widget)fe_GetUserData(widget);

	fe_browse_file_of_text(context, text_field, FALSE);
}

static void
fe_link_tab_remove_link(fe_EditorLinkPropertiesWidgets* w_data)
{
	w_data->url = NULL;

	fe_update_dependents(NULL, w_data->properties, PROP_LINK_HREF);
}

static void
fe_link_href_remove_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorLinkPropertiesWidgets* w_data =
		(fe_EditorLinkPropertiesWidgets*)closure;

	fe_link_tab_remove_link(w_data);
}

static Boolean
fe_link_tab_has_link(fe_EditorLinkPropertiesWidgets* w_data)
{
	char*   link_text;
	Boolean rv = FALSE;

	if (w_data != NULL) {
		/* 
		 *    get the link text, to see if there any.
		 */
		link_text = fe_TextFieldGetString(w_data->link_text);

		if (link_text != NULL) {
			if (link_text[0] != '\0') /* has a link */
				rv = TRUE;
			XP_FREEIF(link_text);
		}
	}
	
	return rv;
}

static void
fe_link_href_remove_update_cb(Widget widget,
							  XtPointer closure, XtPointer call_data)
{
	fe_EditorLinkPropertiesWidgets* w_data =
		(fe_EditorLinkPropertiesWidgets*)closure;
	Boolean sensitive = fe_link_tab_has_link(w_data);

	fe_WidgetSetSensitive(widget, sensitive);
}

#if 0
static void
fe_ListAddItems(Widget widget, char** items, unsigned nitems)
{
	unsigned i;
	XmString xm_string;

	XmListDeleteAllItems(widget);

	for (i = 0; i < nitems; i++) {
		xm_string = XmStringCreateLocalized(items[i]);
		XmListAddItem(widget, xm_string, i + 1); /* MOTIFism */
		XmStringFree(xm_string);
	}
}
#endif

static void
fe_ListSetFromLtabbList(Widget widget, EDT_LtabbList_t ltabb_list)
{
	char*    p;
	XmString xm_string;
	unsigned len;
	unsigned i;

	XmListDeleteAllItems(widget);

	if (ltabb_list) {
		for (p = ltabb_list, i = 0; (len = XP_STRLEN(p)) > 0; i++) {
			xm_string = XmStringCreateLocalized(p);
			XmListAddItem(widget, xm_string, i + 1); /* MOTIFism */
			XmStringFree(xm_string);
			p += len + 1;
		}
	}
}

static char*
fe_LtabbListGetItem(EDT_LtabbList_t ltabb_list, unsigned index)
{
	unsigned i;
	char* p;
	unsigned len;

	if (ltabb_list) {
		for (p = ltabb_list, i = 0; (len = XP_STRLEN(p)) > 0; i++) {
			if (i == index)
				return p;
			p += len + 1;
		}
	}
	return NULL;
}

static void
fe_link_properties_list_cb(Widget widget, XtPointer closure,
									XtPointer foo)
{
    fe_EditorLinkPropertiesWidgets* w_data = 
	  (fe_EditorLinkPropertiesWidgets*)closure;
	XmListCallbackStruct* cb_data = (XmListCallbackStruct*)foo;
	char  basename[MAXPATHLEN];
	char  buf[MAXPATHLEN];
	char* pathname;
	char* p;
	char* target_name;
	Boolean current_doc;

	current_doc = XmToggleButtonGadgetGetState(w_data->target_current_doc);
	
	if (cb_data->reason != XmCR_SINGLE_SELECT)
  	    return;

	if (cb_data->item_position > 0) {
		target_name = fe_LtabbListGetItem(w_data->target_list_data,
										  cb_data->item_position - 1);
	} else {
		target_name = NULL;
	}

	if (current_doc) {
		pathname = NULL;
	} else {
		pathname = fe_TextFieldGetString(w_data->link_text);

		if ((p = strchr(pathname, '#')) != NULL) {
			memcpy(basename, pathname, p - pathname);
			basename[p - pathname] = '\0';
		} else {
			strcpy(basename, pathname);
		}

		XtFree(pathname);
		pathname = basename;

		if (pathname[0] == '\0')
			pathname = NULL;
	}
	
	if (pathname != NULL && target_name != NULL) {
		strcpy(buf, pathname);
		strcat(buf, "#");
		strcat(buf, target_name);
	} else if (target_name != NULL) {
		strcpy(buf, "#");
		strcat(buf, target_name);
	} else {
		buf[0] = '\0';
	}

	fe_TextFieldSetString(w_data->link_text, buf, FALSE);
	w_data->properties->changed |= PROP_LINK_HREF;
}

static void
fe_link_properties_list_update(fe_EditorLinkPropertiesWidgets* w_data,
								  Boolean current_file)
{
	MWContext* context = w_data->properties->context;
	char* link_text;
	EDT_LtabbList_t list_data = NULL;
	char* crazy;
	XmString xm_string;
	int id;

	if (w_data->target_list_data)
		XP_FREE(w_data->target_list_data);
	
	if (current_file) {
		list_data = EDT_GetAllDocumentTargets(context);
	} else {
		link_text = fe_TextFieldGetString(w_data->link_text);
		if (link_text && link_text[0] != '\0') {
			list_data =	EDT_GetAllDocumentTargetsInFile(context, link_text);
		}
		XtFree(link_text);
	}

	if (list_data && XP_STRLEN(list_data) == 0) {
		XP_FREE(list_data);
		list_data = NULL;
	}

	if (list_data && current_file)
		id = XFE_EDITOR_LINK_TARGET_LABEL_CURRENT;
     	/* Link to a named target in the current document (optional.) */
	else if (list_data && !current_file)
		id = XFE_EDITOR_LINK_TARGET_LABEL_SPECIFIED;
	    /* Link to a named target in the specified document (optional.) */
	else
		id = XFE_EDITOR_LINK_TARGET_LABEL_NO_TARGETS;
	    /* No targets in the selected document */

	crazy = XP_GetString(id);
	xm_string = XmStringCreateLocalized(crazy);
	XtVaSetValues(w_data->target_label, XmNlabelString, xm_string, 0);
	XmStringFree(xm_string);
	
	w_data->target_list_data = list_data;

	fe_ListSetFromLtabbList(w_data->target_list, w_data->target_list_data);
	XmToggleButtonGadgetSetState(w_data->target_current_doc,
								 current_file, FALSE);
	XmToggleButtonGadgetSetState(w_data->target_selected_file,
								 !current_file, FALSE);
}

static void
fe_link_properties_list_update_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
	fe_EditorLinkPropertiesWidgets* w_data = 
		(fe_EditorLinkPropertiesWidgets*)closure;
	Boolean set = XmToggleButtonGadgetGetState(w_data->target_current_doc);

	fe_link_properties_list_update(w_data, set);
}

static void
fe_link_properties_target_toggle_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
	fe_EditorLinkPropertiesWidgets* w_data = 
		(fe_EditorLinkPropertiesWidgets*)closure;
	Boolean current_file = (widget == w_data->target_current_doc);
	Boolean set = XmToggleButtonGadgetGetState(widget);
	
	fe_link_properties_list_update(w_data, (set == current_file));
}

/*
 *   Load the form context.
 */
static void
fe_editor_link_properties_dialog_data_init(
									 MWContext* context,
									 fe_EditorLinkPropertiesWidgets* w_data)
{
	char* text;
	char* href;
	char* p;
	Boolean text_exists;

	/*
	 *    If we are on something a link, we display the text that is
	 *    selected, or nothing. Text is read only. If we are not on
	 *    a link, text field is editable.
	 *
	 *    If we are on a link, we display that link, this field
	 *    is always editable.
	 */
	text_exists = fe_EditorHrefGet(context, &text, &href);

	/* if (!text_exists)	
		return;
	 */

	/*
	 *   Replace CR/LF with spaces to avoid ugly break in static display
	 */
	if (text) {
		for (p = text; *p; p++) {
			if (*p == '\n' || *p == '\r')
				*p = ' ';
		}
		w_data->text = text;
	} else {
		w_data->text = NULL;
	}

	if (href) {
		w_data->url = href;
		if (w_data->text == NULL)
			w_data->text = "";
	} else {
		w_data->url = NULL;
	}

	fe_update_dependents(NULL, w_data->properties, PROP_LINK_ALL);

	/*
	 *    Don't know what these are yet.
	 */
	w_data->selected_filename = NULL;
	fe_link_properties_list_update(w_data, TRUE);

	if (text)
		XtFree(text);
	if (href)
		XtFree(href);
}

static void
fe_editor_link_properties_dialog_set(
									 MWContext* context,
									 fe_EditorLinkPropertiesWidgets* w_data)
{
	char* displayed_text;
	char* link_text;

	displayed_text = fe_TextFieldGetString(w_data->displayed_text);
	link_text = fe_TextFieldGetString(w_data->link_text);
	
	/*
	 *    If the text field was editable, then we are creating a new
	 *    link. I don't really like carrying state around in the
	 *    values of the widgets, but this should work ok...djw.
	 */
	if (XmTextFieldGetEditable(w_data->displayed_text)) {
		if (link_text && (link_text[0] != '\0')) /* don't anything if no link! */
			fe_EditorHrefInsert(context, displayed_text, link_text);
	} else { /* modify exiting link/text */
		if (link_text && link_text[0] == '\0')
			fe_EditorHrefSetUrl(context, NULL);
		else
			fe_EditorHrefSetUrl(context, link_text);
	}
	
	if (displayed_text)
		XP_FREEIF(displayed_text);
	if (link_text)
		XP_FREEIF(link_text);
}

static void
fe_make_link_page(
				  MWContext* context,
				  Widget     parent,
				  fe_EditorLinkPropertiesWidgets* w_data
				  )
{
  Arg av [50];
  int ac;
  Widget form;
  Widget linkSourceLabel, sourceFrame, sourceForm;
  Widget linkToLabel, linkToFrame, linkToForm;
  Widget sourceTextLabel;
  Widget linkedTextLabel;
  Widget linkToTarget;
  Widget linkToBrowseFile;
  Widget linkToRemoveLink;
  Widget linkPageTextField;
  Widget sourceTextField;
  Widget target_list;
  Widget SelectedFileToggle;
  Widget currentDocToggle;
  Widget linkToShowTargets;
  Widget kids [100];
  Widget target_rc;
  Widget list_parent;
  Pixel  parent_bg;

  int i;

  XtVaGetValues(parent, XmNbackground, &parent_bg, 0);

#if 0
  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  form = XmCreateForm(parent, "linkProperties", av, ac);
  XtManageChild (form);
#else
  form = parent;
#endif
  w_data->form = form;
    
  /**********************************************************************/
  /*  Create the Link source frame and form 28FEB96RCJ			*/
  /**********************************************************************/
  ac = 0;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  sourceFrame = XmCreateFrame (form, "linkSourceFrame", av, ac);
  ac = 0;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  sourceForm = XmCreateForm (sourceFrame, "linkSourceForm", av, ac);

  /**********************************************************************/
  /*  Create the Link To frame and form 28FEB96RCJ			*/
  /**********************************************************************/
  ac = 0;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg (av [ac], XmNtopWidget, sourceFrame); ac++;
#if 0
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
#endif
  linkToFrame = XmCreateFrame (form, "linkToFrame", av, ac);

  ac = 0;
  XtSetArg (av [ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  linkToForm = XmCreateForm (linkToFrame, "linkToForm", av, ac);

  /**********************************************************************/
  /*  Create the titles for the frames 28FEB96RCJ			*/
  /**********************************************************************/
  ac = 0;
  XtSetArg (av [ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
  linkSourceLabel = XmCreateLabelGadget (sourceFrame, "linkSourceTitle", av, ac);
  linkToLabel = XmCreateLabelGadget (linkToFrame, "linkToTitle", av, ac);

  /**********************************************************************/
  /*  Create and manage the contents of the source form and frame	*/
  /**********************************************************************/
  ac = 0;
  i = 0;
  XtSetArg (av [ac], XmNtopAttachment,   XmATTACH_FORM); ac++;
  XtSetArg (av [ac], XmNleftAttachment,   XmATTACH_FORM); ac++;
  kids [i++] = sourceTextLabel = XmCreateLabelGadget (sourceForm, 
                                                      "linkSourceLabel",
						      av, ac);
  w_data->displayed_label = sourceTextLabel;

  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment,   XmATTACH_WIDGET); ac++;
  XtSetArg (av [ac], XmNtopWidget,       sourceTextLabel); ac++;
  XtSetArg (av [ac], XmNleftAttachment,  XmATTACH_FORM);   ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM);   ac++;
  kids [i++] = sourceTextField = fe_CreateTextField (sourceForm, 
                                                     "linkSourceText", 
                                                     av, ac);
  w_data->displayed_text = sourceTextField;

  XtAddCallback(sourceTextField, XmNvalueChangedCallback,
				fe_link_text_changed_cb, (XtPointer)w_data);
  fe_register_dependent(w_data->properties, sourceTextField,
						FE_MAKE_DEPENDENCY(PROP_LINK_TEXT),
						fe_link_text_update_cb, (XtPointer)w_data);

  XtManageChildren (kids, i);
  XtManageChild (linkSourceLabel);
  XtManageChild (sourceForm);

  /**********************************************************************/
  /*  Create and manage the contents of the Link to form and frame	*/
  /**********************************************************************/
  ac = 0;
  i = 0;
  XtSetArg (av [ac], XmNleftAttachment,   XmATTACH_FORM);   ac++;
  XtSetArg (av [ac], XmNtopAttachment,    XmATTACH_FORM);   ac++;
  kids [i++] = linkedTextLabel = XmCreateLabelGadget (linkToForm, 
                                                    "linkToLabel",
						    av, ac);
  ac = 0;
  XtSetArg (av [ac], XmNleftAttachment,   XmATTACH_WIDGET); ac++;
  XtSetArg (av [ac], XmNleftWidget,       linkedTextLabel); ac++;
  XtSetArg (av [ac], XmNtopAttachment,    XmATTACH_FORM);   ac++;
  kids [i++] = linkToBrowseFile = XmCreatePushButtonGadget (linkToForm,
                                               "browseFile",
                                               av, ac);
  XtAddCallback(linkToBrowseFile, XmNactivateCallback,
				fe_link_href_browse_cb, (XtPointer)w_data);

  ac = 0;
  XtSetArg (av [ac], XmNleftAttachment,   XmATTACH_WIDGET);  ac++;
  XtSetArg (av [ac], XmNleftWidget,       linkToBrowseFile); ac++;
  XtSetArg (av [ac], XmNtopAttachment,    XmATTACH_FORM);    ac++;
  kids [i++] = linkToRemoveLink = XmCreatePushButtonGadget (linkToForm,
                                               "removeLink",
                                               av, ac);
  XtAddCallback(linkToRemoveLink, XmNactivateCallback,
				fe_link_href_remove_cb, (XtPointer)w_data);
  fe_register_dependent(w_data->properties, linkToRemoveLink,
						FE_MAKE_DEPENDENCY(PROP_LINK_HREF),
						fe_link_href_remove_update_cb, (XtPointer)w_data);
  
  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment,   XmATTACH_WIDGET); ac++;
  XtSetArg (av [ac], XmNtopWidget,       linkedTextLabel); ac++;
  XtSetArg (av [ac], XmNtopOffset,       10);              ac++;
  XtSetArg (av [ac], XmNleftAttachment,  XmATTACH_FORM);   ac++;
  XtSetArg (av [ac], XmNrightAttachment, XmATTACH_FORM);   ac++;
  kids [i++] = linkPageTextField = fe_CreateTextField (linkToForm, 
                                                       "linkPageTextField", 
                                                       av, ac);
  w_data->link_text = linkPageTextField;
  XtAddCallback(linkPageTextField, XmNvalueChangedCallback,
				fe_link_href_changed_cb, (XtPointer)w_data);
  fe_register_dependent(w_data->properties, linkPageTextField,
						FE_MAKE_DEPENDENCY(PROP_LINK_HREF),
						fe_link_href_update_cb, (XtPointer)w_data);

  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment,   XmATTACH_WIDGET);   ac++;
  XtSetArg (av [ac], XmNtopWidget,       linkPageTextField); ac++;
  XtSetArg (av [ac], XmNleftAttachment,  XmATTACH_FORM);     ac++;
  XtSetArg (av [ac], XmNalignment, XmALIGNMENT_BEGINNING);   ac++;
  kids [i++] = linkToTarget = XmCreateLabelGadget (linkToForm,
                                                   "linkTarget",
                                                   av, ac);
  w_data->target_label = linkToTarget;

  /*
   *    RowColumn to hold the buttons to right of target text.
   */
  ac = 0;
  XtSetArg (av [ac], XmNorientation, XmVERTICAL);   ac++;
  XtSetArg (av [ac], XmNtopAttachment,   XmATTACH_WIDGET);   ac++;
  XtSetArg (av [ac], XmNtopWidget,       linkToTarget);      ac++;
  XtSetArg (av [ac], XmNrightAttachment,  XmATTACH_FORM);     ac++;
  XtSetArg (av [ac], XmNpacking,  XmPACK_TIGHT);     ac++;
  kids[i++] = target_rc = XmCreateRowColumn(linkToForm,
											"targetRowColumn", av, ac);

  ac = 0;
  XtSetArg (av [ac], XmNtopAttachment,   XmATTACH_WIDGET);   ac++;
  XtSetArg (av [ac], XmNtopWidget,       linkToTarget);      ac++;
  XtSetArg (av [ac], XmNleftAttachment,  XmATTACH_FORM);     ac++;
  XtSetArg (av [ac], XmNrightAttachment,  XmATTACH_WIDGET);     ac++;
  XtSetArg (av [ac], XmNrightWidget,  target_rc);     ac++;
#if 0
  XtSetArg (av [ac], XmNbottomAttachment, XmATTACH_FORM);    ac++;
#endif
  XtSetArg(av[ac], XmNvisibleItemCount, 6); ac++;
  XtSetArg(av[ac], XmNselectionPolicy, XmSINGLE_SELECT); ac++;
  XtSetArg(av[ac], XmNbackground, parent_bg); ac++;
  target_list = XmCreateScrolledList(linkToForm, "targetList",
											 av, ac);
  XtManageChild(target_list);
  kids [i++] = list_parent = XtParent(target_list);

  XtAddCallback(target_list, XmNsingleSelectionCallback,
				fe_link_properties_list_cb, (XtPointer)w_data);
  fe_register_dependent(w_data->properties, target_list,
						FE_MAKE_DEPENDENCY(PROP_LINK_LIST),
						fe_link_properties_list_update_cb, (XtPointer)w_data);

  w_data->target_list = target_list;

  XtManageChildren (kids, i); /* children of form */
  XtManageChild (linkToLabel);
  XtManageChild (linkToForm);

  i = 0; /* no kids */
  ac = 0;
  XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
  kids [i++] = linkToShowTargets = XmCreateLabelGadget(target_rc,
													   "showTargets",
													   av, ac);

  ac = 0;
  XtSetArg (av [ac], XmNalignment, XmALIGNMENT_BEGINNING);     ac++;
  XtSetArg(av[ac], XmNset, TRUE); ac++;
  XtSetArg(av[ac], XmNindicatorType, XmONE_OF_MANY); ac++;
  kids [i++] = currentDocToggle =
    XmCreateToggleButtonGadget(target_rc, "currentDocument", av, ac);

  w_data->target_current_doc = currentDocToggle;

  ac = 0;
  XtSetArg (av [ac], XmNalignment, XmALIGNMENT_BEGINNING);     ac++;
  XtSetArg(av[ac], XmNset, FALSE); ac++;
  XtSetArg(av[ac], XmNindicatorType, XmONE_OF_MANY); ac++;
  kids [i++] = SelectedFileToggle =
    XmCreateToggleButtonGadget (target_rc, "selectedFile", av, ac);

  w_data->target_selected_file = SelectedFileToggle;
  XtManageChildren (kids, i); /* children of rc */

  XtAddCallback(currentDocToggle, XmNvalueChangedCallback,
			  fe_link_properties_target_toggle_cb, (XtPointer)w_data);
  XtAddCallback(SelectedFileToggle, XmNvalueChangedCallback,
			  fe_link_properties_target_toggle_cb, (XtPointer)w_data);

  /**********************************************************************/
  /*  Manage all of the frame comprising the Link dialog box		*/
  /**********************************************************************/

  i = 0;
  kids [i++] = sourceFrame;
  kids [i++] = linkToFrame;
  XtManageChildren (kids, i);

} /* end fe_make_link_page */

static struct fe_style_data fe_style_types[] = {
    { "bold",   TF_BOLD },
    { "italic", TF_ITALIC }, 
    { "underline", TF_UNDERLINE },
    { "fixed",  TF_FIXED },
    { "strikethrough", TF_STRIKEOUT },
    { "superscript",  TF_SUPER },
    { "subscript",    TF_SUB   },
    { "blink",  TF_BLINK },
	{ 0 }
};

static struct fe_style_data fe_font_size_types[] = {
    { "minusTwo",  ED_FONTSIZE_MINUS_TWO },
    { "minusOne",  ED_FONTSIZE_MINUS_ONE },
    { "plusZero",  ED_FONTSIZE_ZERO      },
    { "plusOne",   ED_FONTSIZE_PLUS_ONE  },
    { "plusTwo",   ED_FONTSIZE_PLUS_TWO  },
    { "plusThree", ED_FONTSIZE_PLUS_THREE },
    { "plusFour",  ED_FONTSIZE_PLUS_FOUR },
    { NULL }
};

static void
fe_font_size_update_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;

	if (w_data->js_mode == JAVASCRIPT_NONE) {
		fe_OptionMenuSetHistory(widget, w_data->font_size - 1);
		fe_WidgetSetSensitive(XmOptionButtonGadget(widget), TRUE);
	} else {
		fe_OptionMenuSetHistory(widget, ED_FONTSIZE_ZERO - 1);
		fe_WidgetSetSensitive(XmOptionButtonGadget(widget), FALSE);
	}
	/*FIXME*/ /* fix sensitivity */
}

static void
fe_font_size_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data = (fe_EditorCharacterPropertiesWidgets*)closure;

	ED_FontSize size = (ED_FontSize)fe_GetUserData(widget);

	w_data->font_size = size;
	w_data->changed_mask |= TF_FONT_SIZE;

	fe_update_dependents(widget, w_data->properties, PROP_CHAR_SIZE);
}

static void
fe_style_update_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;
	ED_TextFormat type;
	Boolean set;
	Boolean enabled;
	unsigned foo;

	if (w_data->js_mode == JAVASCRIPT_NONE) {

		foo = (unsigned)fe_GetUserData(widget);
		type = foo;

		if ((type &	w_data->text_attributes) != 0)
			set = TRUE;
		else
			set = FALSE;

		enabled = TRUE;
	} else {
		set = FALSE;
		enabled = FALSE;
	}

	XmToggleButtonGadgetSetState(widget, set, FALSE);
	fe_WidgetSetSensitive(widget, enabled);
}
						 
static void
fe_style_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data =
		(fe_EditorCharacterPropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;
	ED_TextFormat type;
	unsigned foo;

	foo = (unsigned)fe_GetUserData(widget);
	type = foo;
	
	if (info->set) {
		w_data->text_attributes |= type;

		/*
		 *    Super and Sub should be mutually exclusive.
		 */
		if (type == TF_SUPER)
			w_data->text_attributes &= ~TF_SUB;
		else if (type == TF_SUB)
			w_data->text_attributes &= ~TF_SUPER;
	} else {
		w_data->text_attributes &= ~type;
	}

	w_data->changed_mask |= type;

	fe_update_dependents(widget, w_data->properties, PROP_CHAR_STYLE);
}

static void
fe_clear_style_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets*
		w_data = (fe_EditorCharacterPropertiesWidgets*)closure;

	w_data->text_attributes &= ~TF_ALL_MASK;
	w_data->changed_mask |= TF_ALL_MASK;

	fe_update_dependents(widget, w_data->properties, PROP_CHAR_STYLE);
}

static void
fe_clearall_settings_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;
	MWContext* context;
	char* link_text;

	if (w_data->properties->link != NULL) {
	    /* 
		 *    get the link text, to see if there any.
		 */
	    link_text = fe_TextFieldGetString(w_data->properties->link->link_text);

		if (link_text != NULL && link_text[0] != '\0') { /* has a link */
		    context = w_data->properties->context;
			/* Do you want to remove the link? */
		    if (XFE_Confirm(context,
							XP_GetString(XFE_EDITOR_WARNING_REMOVE_LINK)))
				fe_link_tab_remove_link(w_data->properties->link);
		}

		if (link_text != NULL)
		    XP_FREEIF(link_text);
	}

	w_data->js_mode = JAVASCRIPT_NONE;
	w_data->text_attributes &= ~TF_ALL_MASK;
	w_data->is_custom_color = FALSE;
	w_data->font_size = ED_FONTSIZE_ZERO;
	w_data->changed_mask |= TF_FONT_COLOR|TF_FONT_SIZE|TF_ALL_MASK|\
		                    PROP_CHAR_CLIENT|PROP_CHAR_SERVER;

	fe_update_dependents(widget, w_data->properties, PROP_CHAR_ALL);
}

static int
fe_color_do_work(MWContext* context, LO_Color* color_rv)
{
	Widget mainw = CONTEXT_WIDGET(context);
	Widget dialog;
	Arg args[8];
	Cardinal n;
	int done;

	n = 0;
	dialog = fe_CreateColorPickerDialog(mainw, "setColors", args, n);
	fe_ColorPickerDialogSetColor(dialog, color_rv);
	
	/*
	 *   Add a bunch of callbacks to the buttons.
	 */
	XtAddCallback(dialog, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(dialog, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(dialog, XmNdestroyCallback, fe_hrule_destroy_cb, &done);

    /*
     *    Popup.
     */
    XtManageChild(dialog);

	/*
     *    Wait.
     */
	fe_NukeBackingStore(dialog); /* what does this do? */

	done = XmDIALOG_NONE;
	while (done == XmDIALOG_NONE) {
		fe_EventLoop();
	}

	if (done == XmDIALOG_OK_BUTTON)
		fe_ColorPickerDialogGetColor(dialog, color_rv);
	
	if (done != XFE_DIALOG_DESTROY_BUTTON)
		XtDestroyWidget(dialog);
	
	if (done == XmDIALOG_OK_BUTTON)
		return XmDIALOG_OK_BUTTON;
	else
		return XmDIALOG_NONE;
}

static void
fe_color_swatch_update_cb(Widget widget, XtPointer closure,
						  XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;
	LO_Color pooh;
	LO_Color* color;
	Boolean sensitive = FALSE;

	/*FIXME*/
	/*
	 *    I'm not sure how to generate these colors.
	 */
	if (w_data->js_mode == JAVASCRIPT_CLIENT) {
		pooh.red = 255;
		pooh.blue = 0;
		pooh.green = 0;
		color = &pooh;
	} else if (w_data->js_mode == JAVASCRIPT_SERVER) {
		pooh.red = 0;
		pooh.blue = 255;
		pooh.green = 0;
		color = &pooh;
	} else { /* JAVASCRIPT_NONE */

		if (w_data->is_custom_color) {
			color = &w_data->color;
			sensitive = TRUE;
		} else {
			MWContext* context = (MWContext *)fe_WidgetToMWContext(widget);

			fe_EditorDocumentGetColors(context,
									   NULL, /* bg image */
									   NULL,
									   NULL, /* bg color */
									   &pooh,/* normal color */
									   NULL, /* link color */
									   NULL, /* active color */
									   NULL); /* followed color */
			color = &pooh;
		}
	} 

	fe_SwatchSetColor(widget, color);
	fe_SwatchSetSensitive(widget, sensitive);
}

Boolean
fe_ColorPicker(MWContext* context, LO_Color* in_out)
{
	LO_Color color = *in_out;

	if (fe_color_do_work(context, &color) == XmDIALOG_OK_BUTTON) {
		*in_out = color;
		return TRUE;
	}
	return FALSE;
}

static void
fe_editor_props_color_picker_cb(Widget widget,
								XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;
	LO_Color color;

	color = w_data->color;
	if (fe_ColorPicker(w_data->properties->context, &color)) {
		w_data->color = color;
		w_data->is_custom_color = TRUE;
		w_data->changed_mask |= TF_FONT_COLOR;
	}
	fe_update_dependents(widget, w_data->properties, PROP_CHAR_COLOR);
}

static void
fe_color_default_update_cb(Widget widget, XtPointer closure,
						   XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;
	Boolean sensitive = (w_data->js_mode == JAVASCRIPT_NONE);
	Boolean set = !sensitive || (w_data->is_custom_color == FALSE);

	XmToggleButtonSetState(widget, set, FALSE);
	fe_WidgetSetSensitive(widget, sensitive);
}

static void
fe_color_default_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data =
		(fe_EditorCharacterPropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;

	if (info->set) {
		w_data->changed_mask |= TF_FONT_COLOR;
		w_data->is_custom_color = FALSE;
		fe_update_dependents(widget, w_data->properties, PROP_CHAR_COLOR);
	}
}

static void
fe_color_custom_update_cb(Widget widget, XtPointer closure,
						  XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;
	Boolean sensitive = (w_data->js_mode == JAVASCRIPT_NONE);
	Boolean set = sensitive && (w_data->is_custom_color == TRUE);

	XmToggleButtonSetState(widget, set, FALSE);
	fe_WidgetSetSensitive(widget, sensitive);
}

static void
fe_choose_color_update_cb(Widget widget, XtPointer closure,
						  XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;
	Boolean sensitive = (w_data->js_mode == JAVASCRIPT_NONE) &&
                        (w_data->is_custom_color == TRUE);

	fe_WidgetSetSensitive(widget, sensitive);
}

static void
fe_clear_styles_update_cb(Widget widget, XtPointer closure,
						  XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data
		= (fe_EditorCharacterPropertiesWidgets*)closure;
	Boolean sensitive = (w_data->js_mode == JAVASCRIPT_NONE);

	fe_WidgetSetSensitive(widget, sensitive);
}

static void
fe_color_custom_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data =
		(fe_EditorCharacterPropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;

	if (info->set) {
		w_data->changed_mask |= TF_FONT_COLOR;
		w_data->is_custom_color = TRUE;
		fe_update_dependents(widget, w_data->properties, PROP_CHAR_COLOR);
	}
}

static void
fe_javascript_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data =
		(fe_EditorCharacterPropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;
	Boolean is_server = (fe_GetUserData(widget) != NULL);

	if (info->set) {
		if (is_server) {
			w_data->js_mode = JAVASCRIPT_SERVER;
		} else {
			w_data->js_mode = JAVASCRIPT_CLIENT;
		}

		/*
		 *    Javascript text doesn't have links, clear it..
		 */
		if (w_data->properties->link != NULL)
			fe_link_tab_remove_link(w_data->properties->link);
		
	} else {
		w_data->js_mode = JAVASCRIPT_NONE;
	}
	w_data->changed_mask |= PROP_CHAR_CLIENT|PROP_CHAR_SERVER;

	fe_update_dependents(widget, w_data->properties, PROP_CHAR_ALL);
}

static void
fe_javascript_update_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorCharacterPropertiesWidgets* w_data =
		(fe_EditorCharacterPropertiesWidgets*)closure;
	Boolean is_server = (fe_GetUserData(widget) != NULL);
	Boolean set = FALSE;

	if (is_server && (w_data->js_mode == JAVASCRIPT_SERVER))
		set = TRUE;
	else if (!is_server && (w_data->js_mode == JAVASCRIPT_CLIENT))
		set = TRUE;
	
	XmToggleButtonGadgetSetState(widget, set, FALSE);
}

static void
fe_EditorCharacterPropertiesDialogDataGet(
								 MWContext* context,
								 fe_EditorCharacterPropertiesWidgets* w_data)
{
	w_data->text_attributes = fe_EditorCharacterPropertiesGet(context);
	w_data->font_size = fe_EditorFontSizeGet(context);
	w_data->changed_mask = 0;

	if ((w_data->text_attributes & TF_SERVER))
		w_data->js_mode = JAVASCRIPT_SERVER;
	else if ((w_data->text_attributes & TF_SCRIPT))
		w_data->js_mode = JAVASCRIPT_CLIENT;
	else
		w_data->js_mode = JAVASCRIPT_NONE;
	w_data->text_attributes &= ~(TF_SERVER|TF_SCRIPT);

	if (fe_EditorColorGet(context, &w_data->color))
		w_data->is_custom_color = TRUE;
	else
		w_data->is_custom_color = FALSE;

	fe_update_dependents(NULL, w_data->properties, PROP_CHAR_ALL);
}

static void
fe_EditorCharacterPropertiesDialogSet(
								 MWContext* context,
								 fe_EditorCharacterPropertiesWidgets* w_data)
{
	LO_Color* color;

	if ((w_data->js_mode == JAVASCRIPT_SERVER)) {
		fe_EditorCharacterPropertiesSet(context, TF_SERVER);
	} else if ((w_data->js_mode == JAVASCRIPT_CLIENT)) {
		fe_EditorCharacterPropertiesSet(context, TF_SCRIPT);
	} else {

		/* clear javascript */
		if (w_data->changed_mask & (PROP_CHAR_CLIENT|PROP_CHAR_SERVER)) {
			fe_EditorCharacterPropertiesSet(context, (TF_SERVER|TF_SCRIPT));
			w_data->changed_mask = ~0;
		}

		if ((w_data->changed_mask & TF_ALL_MASK)!= 0) {
			fe_EditorCharacterPropertiesSet(context, 
									(w_data->text_attributes & TF_ALL_MASK));
		}

		if ((w_data->changed_mask & TF_FONT_SIZE)!= 0)
			fe_EditorFontSizeSet(context, w_data->font_size);

		if ((w_data->changed_mask & TF_FONT_COLOR)!= 0) {
			
			if (w_data->is_custom_color)
				color = &w_data->color;
			else
				color = NULL;

			fe_EditorColorSet(context, color);
		}
	}
}

static void
fe_make_character_page(
					   MWContext *context,
					   Widget parent,
					   fe_EditorCharacterPropertiesWidgets* w_data)
{
	Arg    args[32];
	Cardinal n;
	Widget form;
	Widget color_frame;
	Widget color_form;
	Widget color_label;
	Widget color_swatch;
	Widget color_default;
	Widget color_custom;
	Widget color_choose;
	Widget color_text;

	Widget size_frame;
	Widget size_form;
	Widget size_menu;
	Widget size_text;
	Widget size_cascade;

	Widget style_frame;
	Widget style_outer_rc;
	Widget style_inner_rc;
	Widget style_clear;

	Widget java_frame;
	Widget java_rc;
	
	Widget clear_all;

	Dimension width;
	Dimension height;

	Widget children[16];
	Cardinal nchildren;

#if 0
	n = 0;
	form = XmCreateForm(parent, "characterProperties", args, n);
	XtManageChild(form);
#else
	form = parent;
#endif
	w_data->form = form;

	/*
	 *    Color group.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	color_frame = fe_CreateFrame(form, "color", args, n);
	XtManageChild(color_frame);

	n = 0;
	color_form = XmCreateForm(color_frame, "color", args, n);
	XtManageChild(color_form);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	children[nchildren++] = color_label = XmCreateLabelGadget(color_form, "colorLabel", args, n);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, color_label); n++;
	XtSetArg(args[n], XmNshadowThickness, 2); n++; /* make it look like label */
	children[nchildren++] = color_swatch = fe_CreateSwatchButton(color_form, "colorSwatch", args, n);
	w_data->color_swatch = color_swatch;

	fe_register_dependent(w_data->properties, color_swatch,
								 FE_MAKE_DEPENDENCY(PROP_CHAR_COLOR),
								 fe_color_swatch_update_cb, (XtPointer)w_data);
	XtAddCallback(color_swatch, XmNactivateCallback,
				  fe_editor_props_color_picker_cb, (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, color_label); n++;
	XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	XtSetArg(args[n], XmNset, TRUE); n++;
	children[nchildren++] = color_default = XmCreateToggleButtonGadget(color_form, "default", args, n);

	XtAddCallback(color_default, XmNvalueChangedCallback,
								 fe_color_default_cb, (XtPointer)w_data);

	fe_register_dependent(w_data->properties, color_default,
								 FE_MAKE_DEPENDENCY(PROP_CHAR_COLOR),
								 fe_color_default_update_cb, (XtPointer)w_data);
	
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, color_default); n++;
	XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	XtSetArg(args[n], XmNset, FALSE); n++;
	children[nchildren++] = color_custom = XmCreateToggleButtonGadget(color_form, "custom", args, n);
	
	XtAddCallback(color_custom, XmNvalueChangedCallback,
								 fe_color_custom_cb, (XtPointer)w_data);

	fe_register_dependent(w_data->properties, color_custom,
								 FE_MAKE_DEPENDENCY(PROP_CHAR_COLOR),
								 fe_color_custom_update_cb, (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, color_default); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, color_custom); n++;
	children[nchildren++] = color_choose = XmCreatePushButtonGadget(color_form, "chooseColor", args, n);
	 
	XtAddCallback(color_choose, XmNactivateCallback,
				  fe_editor_props_color_picker_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, color_choose,
						  FE_MAKE_DEPENDENCY(PROP_CHAR_COLOR),
						  fe_choose_color_update_cb, (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, color_custom); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	children[nchildren++] = color_text = XmCreateLabelGadget(color_form, "colorText", args, n);

	XtManageChildren(children, nchildren); /* children of color form */

	/*
	 *    Set the swatch to be same height as label, and width as
	 *    choose button.
	 */
	XtVaGetValues(color_label, XmNheight, &height, 0);
	XtVaGetValues(color_choose, XmNwidth, &width, 0);
	n = 0;
	XtSetArg(args[n], XmNwidth, width); n++;
	XtSetArg(args[n], XmNheight, height); n++;
	XtSetValues(color_swatch, args, n);

	/*
	 *    Size group.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, color_frame); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, color_frame); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	size_frame = fe_CreateFrame(form, "size", args, n);
	XtManageChild(size_frame);

	n = 0;
	size_form = XmCreateForm(size_frame, "size", args, n);
	XtManageChild(size_form);
	
	nchildren = 0;
	n = 0;
	size_menu = fe_CreatePulldownMenu(size_form,
									  "fontSize",
									  args, n);

	nchildren = 0;
	for (nchildren = 0; fe_font_size_types[nchildren].name; nchildren++) {
		n = 0;
		XtSetArg(args[n], XmNuserData, fe_font_size_types[nchildren].data); n++;
		children[nchildren] = XmCreatePushButtonGadget(
												   size_menu,
												   fe_font_size_types[nchildren].name,
												   args,
												   n
												   );
		XtAddCallback(children[nchildren], XmNactivateCallback,
					  fe_font_size_cb, (XtPointer)w_data);
															   
	}
	XtManageChildren(children, nchildren); /* size buttons */
	
	n = 0;
	XtSetArg(args[n], XmNsubMenuId, size_menu); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	size_cascade = fe_CreateOptionMenu(
									  size_form,
									  "fontSizeOptionMenu",
									  args, n);
	fe_UnmanageChild_safe(XmOptionLabelGadget(size_cascade));
	fe_register_dependent(w_data->properties, size_cascade,
					 FE_MAKE_DEPENDENCY(PROP_CHAR_SIZE),
					 fe_font_size_update_cb, (XtPointer)w_data);

	XtManageChild(size_cascade);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, size_cascade); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	size_text = XmCreateLabelGadget(size_form, "sizeText",
									args, n);
	XtManageChild(size_text);

	/*
	 *    Style group.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, color_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, color_frame); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	style_frame = fe_CreateFrame(form, "style", args, n);
	XtManageChild(style_frame);

	n = 0;
	style_outer_rc = XmCreateForm(style_frame, "style", args, n);
	XtManageChild(style_outer_rc);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	XtSetArg(args[n], XmNnumColumns, 2); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	style_inner_rc = XmCreateRowColumn(style_outer_rc, "charProperties", args, n);
	XtManageChild(style_inner_rc);

	for (nchildren = 0; fe_style_types[nchildren].name; nchildren++) {
		n = 0;
		XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); n++;
		XtSetArg(args[n], XmNuserData, fe_style_types[nchildren].data); n++;
		children[nchildren] = XmCreateToggleButtonGadget(
											 style_inner_rc,
											 fe_style_types[nchildren].name,
											 args,
											 n);
															   
		XtAddCallback(children[nchildren], XmNvalueChangedCallback,
					  fe_style_cb, (XtPointer)w_data);

		fe_register_dependent(w_data->properties, children[nchildren],
						  FE_MAKE_DEPENDENCY(fe_style_types[nchildren].data),
						  fe_style_update_cb, (XtPointer)w_data);
	}
	XtManageChildren(children, nchildren); /* style toggles */

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, style_inner_rc); n++;
	style_clear = XmCreatePushButtonGadget(style_outer_rc, "clearStyles",
										   args, n);
	XtAddCallback(style_clear, XmNactivateCallback,
				  fe_clear_style_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, style_clear,
						  FE_MAKE_DEPENDENCY(PROP_CHAR_JAVASCRIPT),
						  fe_clear_styles_update_cb, (XtPointer)w_data);

	XtManageChild(style_clear);

	/*
	 *    Java group.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, color_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, color_frame); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	java_frame = fe_CreateFrame(form, "java", args, n);
	XtManageChild(java_frame);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	java_rc = XmCreateRowColumn(java_frame, "java", args, n);
	XtManageChild(java_rc);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); n++;
	XtSetArg(args[n], XmNuserData, FALSE); n++;
	children[nchildren] = XmCreateToggleButtonGadget(java_rc, "client",
													 args, n);
	
	XtAddCallback(children[nchildren], XmNvalueChangedCallback,
				  fe_javascript_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, children[nchildren],
						  FE_MAKE_DEPENDENCY(PROP_CHAR_JAVASCRIPT), /* lazy */
						  fe_javascript_update_cb, (XtPointer)w_data);
	nchildren++;

	n = 0;
	XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); n++;
	XtSetArg(args[n], XmNuserData, TRUE); n++;
	children[nchildren] = XmCreateToggleButtonGadget(java_rc, "server",
													   args, n);
	XtAddCallback(children[nchildren], XmNvalueChangedCallback,
				  fe_javascript_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, children[nchildren],
						  FE_MAKE_DEPENDENCY(PROP_CHAR_JAVASCRIPT), /* lazy */
						  fe_javascript_update_cb, (XtPointer)w_data);
	nchildren++;

	XtManageChildren(children, nchildren); /* java toggles */

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, java_frame); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, java_frame); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, java_frame); n++;
	clear_all = XmCreatePushButtonGadget(form, "clearAll", args, n);
	XtManageChild(clear_all);

	XtAddCallback(clear_all, XmNactivateCallback,
				  fe_clearall_settings_cb, (XtPointer)w_data);
}

static void
fe_image_main_image_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;

	/*
	 *    Update changed tag, but don't call dependents (that's us).
	 *    Well we have to, so that we update the copy button.
	 *    No, we don't we'll set the tag here, but we'll only
	 *    call update_dependents from the browse cb. That way
	 *    we won't be *busy*. No, we have to, oh too bad!
	 */
	w_data->new_image = TRUE;
	fe_update_dependents(widget, w_data->properties, 
						 PROP_IMAGE_MAIN_IMAGE|PROP_IMAGE_COPY|PROP_LINK_TEXT);
}

static void
fe_image_main_image_update_cb(Widget widget,
							  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;

	if (info->caller == widget)
		return; /* don't call ourselves */
	
	fe_set_text_field(widget, w_data->image_data.pSrc,
					  fe_image_main_image_cb, (XtPointer)w_data);
}

static void
fe_url_browse_file_of_text(MWContext* context, Widget text_field)
{
    char* before_save;
	char* after;
	char* p = NULL;
	int type;

	before_save = fe_TextFieldGetString(text_field);

	if (before_save != NULL)
		fe_StringTrim(before_save);

	type = NET_URL_Type(before_save);

	if (type == 0) { /* unknown, might be file path */
		History_entry* hist_ent;

		if (!EDT_IS_NEW_DOCUMENT(context)) { /* don't bother looking */
			hist_ent = SHIST_GetCurrent(&context->hist);

			if (hist_ent != NULL
				&&
				hist_ent->address != NULL
				&&
				NET_IsLocalFileURL(hist_ent->address)) {
				if (before_save[0] == '/' && before_save[1] != '/')
					p = before_save;
			}
		}
	} else if (type == FILE_TYPE_URL) {

		if (XP_STRNCASECMP(before_save, "file:///", 8) == 0)
			p = &before_save[7];
		else if (XP_STRNCASECMP(before_save, "file://", 7) == 0)
			p = &before_save[6];
		else if (XP_STRNCASECMP(before_save, "file:/", 6) == 0)
			p = &before_save[5];
	}

	if (p == NULL)
		p = "";

	if (p != before_save)
		fe_TextFieldSetString(text_field, p, FALSE);
	
	fe_browse_file_of_text(context, text_field, FALSE);

	after = fe_TextFieldGetString(text_field);
	fe_StringTrim(after);

	if (XP_STRCMP(after, before_save) != 0) {

		p = (char*)XP_ALLOC(XP_STRLEN(after) + 8);

		XP_STRCPY(p, "file://");
		XP_STRCAT(p, after);

		fe_TextFieldSetString(text_field, p, FALSE);

		XP_FREE(p);
	}

	if (before_save)
		XtFree(before_save);
	if (after)
		XtFree(after);
}

static void
fe_image_main_image_browse_cb(Widget widget,
							  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;

	fe_url_browse_file_of_text(w_data->properties->context,
							   w_data->main_image);
}

static void
fe_image_alt_image_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;

	/*
	 *    See discussion for main above.
	 */
	w_data->new_image = TRUE;
	fe_update_dependents(widget, w_data->properties, 
						 PROP_IMAGE_ALT_IMAGE|PROP_IMAGE_COPY|PROP_LINK_TEXT);
}

static void
fe_image_alt_image_update_cb(Widget widget,
							  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;

	if (info->caller == widget)
		return; /* don't call ourselves */
	
	fe_set_text_field(w_data->alt_image, w_data->image_data.pLowSrc,
					  fe_image_alt_image_cb, (XtPointer)w_data);
}

static void
fe_image_alt_image_browse_cb(Widget widget,
							  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;

	fe_url_browse_file_of_text(w_data->properties->context,
							   w_data->alt_image);
}

static void
fe_image_alt_text_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;

	fe_update_dependents(widget, w_data->properties,
						 PROP_IMAGE_ALT_TEXT|PROP_IMAGE_COPY|PROP_LINK_TEXT);
}

static void
fe_image_alt_text_update_cb(Widget widget,
							  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;

	if (info->caller == widget)
		return; /* don't call ourselves */
	
	fe_set_text_field(w_data->alt_text, w_data->image_data.pAlt,
					  fe_image_alt_text_cb, (XtPointer)w_data);
}

static void
fe_image_align_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;

	/*
	 *    We get a callback on both the unset of the old toggle,
	 *    and the set of the new, so ignore the former, it's not
	 *    interesting.
	 */
	if (info->set) {

		w_data->image_data.align = (ED_Alignment)fe_GetUserData(widget);	

		fe_update_dependents(widget, w_data->properties, PROP_IMAGE_ALIGN);
	}
}

static void
fe_image_align_update_cb(Widget widget,
						 XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	ED_Alignment align = (ED_Alignment)fe_GetUserData(widget);	

	XmToggleButtonGadgetSetState(widget, 
								 (w_data->image_data.align == align),
								 FALSE);
}

static void
fe_image_dimensions_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	Boolean constrainable =	!(w_data->image_data.bHeightPercent ||
							  w_data->image_data.bWidthPercent);

	w_data->properties->changed |= PROP_IMAGE_DIMENSIONS;

	/*
	 *    If constrain is on, adjust the other value.
	 */
	if (w_data->do_constrain && constrainable) {
		unsigned long new_value = fe_get_numeric_text_field(widget);
		unsigned long tmp;
		unsigned long foo;
		if (widget == w_data->image_width) { /* adjusting width, do height */
			w_data->image_data.iWidth = new_value;
			foo = w_data->image_data.iOriginalWidth;
			if (foo == 0)
				foo++;
			tmp = new_value * w_data->image_data.iOriginalHeight;
			tmp = tmp / foo;
			w_data->image_data.iHeight = tmp;
		} else { /* adjusting height, do width */
			w_data->image_data.iHeight = new_value;
			foo = w_data->image_data.iOriginalHeight;
			if (foo == 0)
				foo++;
			tmp = new_value * w_data->image_data.iOriginalWidth;
			tmp = tmp / foo;
			w_data->image_data.iWidth = tmp;
		}
		fe_update_dependents(widget, w_data->properties,
							 PROP_IMAGE_DIMENSIONS);
	}
}

static void
fe_image_dimensions_height_update_cb(Widget widget,
									 XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;
	Boolean enabled = w_data->do_custom_size;
	unsigned height; 

	if (info->caller == widget)
		return; /* don't call ourselves */

	if (enabled)
		height = w_data->image_data.iHeight;
	else
		height = w_data->image_data.iOriginalHeight;
	
	fe_set_numeric_text_field(widget, height);
	fe_TextFieldSetEditable(w_data->properties->context, widget, enabled);
}

static void
fe_image_dimensions_width_update_cb(Widget widget,
									 XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;
	Boolean enabled = w_data->do_custom_size;
	unsigned width;

	if (info->caller == widget)
		return; /* don't call ourselves */
	
	if (enabled)
		width = w_data->image_data.iWidth;
	else
		width = w_data->image_data.iOriginalWidth;
	
	fe_set_numeric_text_field(widget, width);
	fe_TextFieldSetEditable(w_data->properties->context, widget, enabled);
}

static void
fe_image_margin_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;

	w_data->properties->changed |= PROP_IMAGE_SPACE;
}

static void
fe_image_margin_height_update_cb(Widget widget,
									 XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;

	if (info->caller == widget)
		return; /* don't call ourselves */
	
	fe_set_numeric_text_field(widget, w_data->image_data.iVSpace);
}

static void
fe_image_margin_width_update_cb(Widget widget,
									 XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;

	if (info->caller == widget)
		return; /* don't call ourselves */

	fe_set_numeric_text_field(widget,
							  w_data->image_data.iHSpace);
}

static void
fe_image_margin_border_update_cb(Widget widget,
								 XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)call_data;

	if (info->caller == widget)
		return; /* don't call ourselves */

	/* Logic for dealing with w_data->image_data.iBorder possibly being -1
	   is now in fe_EditorImagePropertiesDialogDataGet. */

	fe_set_numeric_text_field(widget, w_data->image_data.iBorder);
}

static void
fe_image_original_size_cb(Widget widget,
						  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;

	w_data->do_custom_size = (info->set == FALSE);
	fe_update_dependents(widget, w_data->properties, PROP_IMAGE_DIMENSIONS);
}

static void
fe_image_original_size_update_cb(Widget widget,
						  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	Boolean set  = !w_data->do_custom_size;
	Boolean enabled = w_data->existing_image || w_data->new_image;

	XtVaSetValues(widget, XmNset, set, XmNsensitive, enabled, 0);
}

static void
fe_image_custom_size_cb(Widget widget,
						  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;

	w_data->do_custom_size = info->set;
	fe_update_dependents(widget, w_data->properties, PROP_IMAGE_DIMENSIONS);
}

static void
fe_image_custom_size_update_cb(Widget widget,
						  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	Boolean set  = w_data->do_custom_size;
	Boolean enabled = w_data->existing_image || w_data->new_image;

	XtVaSetValues(widget, XmNset, set, XmNsensitive, enabled, 0);
}

static void
fe_image_copy_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;

	w_data->image_data.bNoSave = info->set;
	fe_update_dependents(widget, w_data->properties, PROP_IMAGE_COPY);
}

static void
fe_image_copy_update_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	Arg args[8];
	Cardinal n;

	n = 0;
	XtSetArg(args[n], XmNset, (w_data->image_data.bNoSave)); n++;
	XtSetValues(widget, args, n);
}

static void
fe_image_remove_imap_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;

	w_data->image_data.bIsMap = FALSE;
	fe_update_dependents(widget, w_data->properties, PROP_IMAGE_IMAP);
}

static void
fe_image_remove_imap_update_cb(Widget widget,
							   XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	Arg args[8];
	Cardinal n;

	n = 0;
	XtSetArg(args[n], XmNsensitive, (w_data->image_data.bIsMap != FALSE)); n++;
	XtSetValues(widget, args, n);
}

static void
fe_image_edit_image_do(MWContext* context,
					   Widget widget,
					   Widget image_widget)
{
    History_entry* hist_ent = SHIST_GetCurrent(&(context->hist));
	char* image;
	char* url;
	char* local_name;
	char buf[512];

    if (hist_ent == NULL || hist_ent->address == NULL)
        return; /* should not happen */

    image = fe_TextFieldGetString(image_widget);

    if (image == NULL || image[0] == '\0')
        return; /* also shouldn't happen */

    url = NET_MakeAbsoluteURL(hist_ent->address, image); /* alloc */

    if (NET_IsHTTP_URL(url)) { /* remote url */
        fe_error_dialog(context, widget,
                        XP_GetString(XFE_EDITOR_IMAGE_IS_REMOTE));
        return;
    }

    if (XP_ConvertUrlToLocalFile(url, &local_name)) {
		fe_EditorEditImage(context, local_name);
	} else {
		sprintf(buf, XP_GetString(XFE_CANNOT_READ_FILE), image);

		fe_error_dialog(context, widget, buf);
	}

	if (image)
		XP_FREEIF(image);
	if (url)
		XtFree(url);
	if (local_name)
		XtFree(local_name);
}

static void
fe_image_edit_image_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	MWContext* context = w_data->properties->context;

	fe_image_edit_image_do(context, widget, w_data->main_image);
}

static void
fe_image_edit_image_update_cb(Widget widget,
							   XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	Boolean sensitive = XmTextFieldGetLastPosition(w_data->main_image) != 0;

	XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}

static void
fe_image_edit_alt_image_cb(Widget widget,
						   XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	MWContext* context = w_data->properties->context;

	fe_image_edit_image_do(context, widget, w_data->alt_image);
}

static void
fe_image_edit_alt_image_update_cb(Widget widget,
								  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	Boolean sensitive = XmTextFieldGetLastPosition(w_data->alt_image) != 0;

	XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}

static void
fe_image_height_pixel_percent_cb(Widget widget,
								 XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	XP_Bool percent = (strcmp(XtName(widget), "percent") == 0);

	w_data->image_data.bHeightPercent = percent;
	fe_update_dependents(widget, w_data->properties, PROP_IMAGE_HEIGHT);
}

static void
fe_image_width_pixel_percent_cb(Widget widget,
								XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	XP_Bool percent = (strcmp(XtName(widget), "percent") == 0);

	w_data->image_data.bWidthPercent = percent;
	fe_update_dependents(widget, w_data->properties, PROP_IMAGE_WIDTH);
}

/*
 *    Forward decalres for stuff used around here, because I'm just too
 *    tired to move stuff aorund.
 */
char* fe_SimpleOptionPixelPercent[];
Widget fe_CreateSimpleOptionMenu(Widget, char*, Arg*, Cardinal);
								  
static void
fe_image_height_pixel_percent_update_cb(Widget widget,
								XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	unsigned index = 0;
	Boolean enabled = w_data->do_custom_size;

	if (enabled && w_data->image_data.bHeightPercent)
		index = 1;

	XfeOptionMenuSetItem(widget, index);
	XtVaSetValues(widget, XmNsensitive, enabled, 0);
}

static void
fe_image_width_pixel_percent_update_cb(Widget widget,
								XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	unsigned index = 0;
	Boolean enabled = w_data->do_custom_size;

	if (enabled && w_data->image_data.bWidthPercent)
		index = 1;

	XfeOptionMenuSetItem(widget, index);
	XtVaSetValues(widget, XmNsensitive, enabled, 0);
}

static void
fe_image_constrain_cb(Widget widget,
					  XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	XmToggleButtonCallbackStruct* info =
		(XmToggleButtonCallbackStruct*)call_data;

	w_data->do_constrain = info->set;
}

static void
fe_image_constrain_update_cb(Widget widget,
							 XtPointer closure, XtPointer call_data)
{
	fe_EditorImagePropertiesWidgets* w_data =
		(fe_EditorImagePropertiesWidgets*)closure;
	Boolean enabled = w_data->do_custom_size;
	Boolean sensitive =	!(w_data->image_data.bHeightPercent ||
						  w_data->image_data.bWidthPercent);
	Boolean set = (w_data->do_constrain) && sensitive;

#if 0
	if (info->caller == widget)
		return; /* don't call ourselves */
#endif

	XtVaSetValues(widget,
				  XmNsensitive, (sensitive && enabled),
				  XmNset, set,
				  0);
}

static void
fe_EditorImagePropertiesDialogDataGet(MWContext* context,
								  fe_EditorImagePropertiesWidgets* w_data)
{
	EDT_ImageData* old = NULL;

	if (EDT_GetCurrentElementType(context) == ED_ELEMENT_IMAGE) {
		old = EDT_GetImageData(context);
		memcpy(&w_data->image_data, old, sizeof(EDT_ImageData));

		if (w_data->image_data.iOriginalWidth != w_data->image_data.iWidth
			||
			w_data->image_data.iOriginalHeight != w_data->image_data.iHeight)
			w_data->do_custom_size = TRUE;
		else
			w_data->do_custom_size = FALSE;
		w_data->existing_image = TRUE;

	} else {
		old = EDT_NewImageData();
		memcpy(&w_data->image_data, old, sizeof(EDT_ImageData));
		w_data->do_custom_size = FALSE;
		w_data->existing_image = FALSE;
	}

	w_data->new_image = FALSE;
	w_data->do_constrain = TRUE; /* ooo, we like this */

	if (w_data->image_data.iBorder == -1) {
		w_data->default_border = TRUE;
		w_data->image_data.iBorder = EDT_GetDefaultBorderWidth(context);
	}
	else {
		w_data->default_border = FALSE;
	}

	fe_update_dependents(NULL, w_data->properties, PROP_IMAGE_ALL);
	/* yes, that's all sally */

	if (old)
		EDT_FreeImageData(old);
}

static void
fe_copy_string_over(char** tgt, Widget widget)
{
	char* p = fe_TextFieldGetString(widget);
	
	if (*tgt)
		XP_FREE(*tgt);
	if (p && *p != '\0')
		*tgt = XP_STRDUP(p);
	else
		*tgt = NULL;
	if (p)
		XtFree(p);
}


static void
fe_editor_image_properties_common_set(MWContext* context,
									  EDT_ImageData* data,
									  fe_EditorImagePropertiesWidgets* w_data)
{
	if (w_data->do_custom_size) {
		/*
		 *    probably don't nee dto do this now, as the valueChanged
		 *    callbacks keep this up to date.
		 */
		data->iWidth = fe_get_numeric_text_field(w_data->image_width);
		data->iHeight = fe_get_numeric_text_field(w_data->image_height);
		data->bWidthPercent = w_data->image_data.bWidthPercent;;
		data->bHeightPercent = w_data->image_data.bHeightPercent;
	} else {
		data->iWidth = w_data->image_data.iOriginalWidth;
		data->iHeight = w_data->image_data.iOriginalHeight;
		data->bWidthPercent = FALSE;
		data->bHeightPercent = FALSE;
	}
	data->iHSpace = fe_get_numeric_text_field(w_data->margin_width);
	data->iVSpace = fe_get_numeric_text_field(w_data->margin_height);
	data->iBorder = fe_get_numeric_text_field(w_data->margin_solid);
}

static Boolean
fe_editor_image_properties_validate(MWContext* context,
								  fe_EditorImagePropertiesWidgets* w_data)
{
	EDT_ImageData  image_data;
	EDT_ImageData* i = &image_data;
	unsigned       errors[16];
	unsigned       nerrors = 0;
	
	fe_editor_image_properties_common_set(context, &image_data, w_data);

	if (w_data->do_custom_size) {
		
		unsigned low;
		unsigned high;
	
		if (i->bWidthPercent) {
			low = TABLE_MIN_PERCENT_WIDTH;
			high = TABLE_MAX_PERCENT_WIDTH;
		} else {
			low = IMAGE_MIN_WIDTH;
			high = IMAGE_MAX_WIDTH;
		}
	
		if (RANGE_CHECK(i->iWidth, low, high))
			errors[nerrors++] = XFE_EDITOR_TABLE_WIDTH_RANGE;

		if (i->bHeightPercent) {
			low = TABLE_MIN_PERCENT_HEIGHT;
			high = TABLE_MAX_PERCENT_HEIGHT;
		} else {
			low = IMAGE_MIN_HEIGHT;
			high = IMAGE_MAX_HEIGHT;
		}

		if (RANGE_CHECK(i->iHeight, low, high))
			errors[nerrors++] = XFE_EDITOR_TABLE_HEIGHT_RANGE;
	}
	
	if (RANGE_CHECK(i->iHSpace, IMAGE_MIN_HSPACE, IMAGE_MAX_HSPACE))
		errors[nerrors++] = XFE_INVALID_IMAGE_HSPACE;
	if (RANGE_CHECK(i->iVSpace, IMAGE_MIN_VSPACE, IMAGE_MAX_VSPACE))
		errors[nerrors++] = XFE_INVALID_IMAGE_VSPACE;
	if (RANGE_CHECK(i->iBorder, IMAGE_MIN_BORDER, IMAGE_MAX_BORDER))
		errors[nerrors++] = XFE_INVALID_IMAGE_BORDER;

	if (nerrors > 0) {
		fe_editor_range_error_dialog(context, w_data->image_width,
									 errors, nerrors);
		return FALSE;
	}

	return TRUE;
}

static void
fe_editor_image_properties_set(MWContext* context,
                               fe_EditorImagePropertiesWidgets* w_data)
{
	EDT_ImageData* old;

	if (w_data->existing_image)
		old = EDT_GetImageData(context);
	else
		old = EDT_NewImageData();

	if ((w_data->properties->changed & PROP_IMAGE_MAIN_IMAGE) != 0)
		fe_copy_string_over(&old->pSrc, w_data->main_image);
	
	if ((w_data->properties->changed & PROP_IMAGE_ALT_IMAGE) != 0)
		fe_copy_string_over(&old->pLowSrc, w_data->alt_image);

	if ((w_data->properties->changed & PROP_IMAGE_ALT_TEXT) != 0)
		fe_copy_string_over(&old->pAlt, w_data->alt_text);
        else if (old->pAlt == 0) {
            old->pAlt = XP_STRDUP(EDT_GetFilename(old->pSrc, FALSE));
            fe_set_text_field(w_data->alt_text, w_data->image_data.pAlt,
                              fe_image_alt_text_cb, (XtPointer)w_data);
        }
	
	fe_editor_image_properties_common_set(context, old, w_data);

	old->align = w_data->image_data.align;
	old->bIsMap = w_data->image_data.bIsMap;
	old->bNoSave = w_data->image_data.bNoSave;

	if (w_data->default_border && old->iBorder == w_data->image_data.iBorder) {
		old->iBorder = -1;
	}


	/*
	 *    We don't handle the link data here, let the link sheet
	 *    do that.
	 */
	if (old->pSrc != NULL) {
		if (w_data->existing_image)
			EDT_SetImageData(context, old, !w_data->image_data.bNoSave);
		else
			EDT_InsertImage(context, old, !w_data->image_data.bNoSave);
	}

	EDT_FreeImageData(old);
}

static struct fe_style_data fe_image_button_names[] = {
 	{ "alignTop",       ED_ALIGN_TOP       },
	{ "alignAbsCenter", ED_ALIGN_ABSCENTER },
	{ "alignCenter",    ED_ALIGN_CENTER    },
	{ "alignBaseline",  ED_ALIGN_BASELINE  },
	{ "alignAbsBottom", ED_ALIGN_ABSBOTTOM },
	{ "alignLeft",      ED_ALIGN_LEFT      },
	{ "alignRight",     ED_ALIGN_RIGHT     },
	{ 0 }

#if 0
	{ "alignAbsTop",    ED_ALIGN_ABSTOP    },
	{ "alignBottom",    ED_ALIGN_BOTTOM    },
#endif
};

static void
fe_make_image_icons(MWContext *context, Widget parent,
					fe_EditorImagePropertiesWidgets* w_data)
{
	Pixmap   p;
	Arg      args[16];
	Cardinal n;
	Cardinal i;
	Widget   children[16];
	char*    name;

	for (i = 0; fe_image_button_names[i].name; i++) {
	  
		MWContextType old = context->type;

		context->type = MWContextEditor;
		p  = fe_ToolbarPixmap(context, IL_ALIGN1_RAISED+(2*i), False, False);
		context->type = old;

		name = fe_image_button_names[i].name;

		n = 0;
		XtSetArg(args[n], XmNlabelType, XmPIXMAP); n++;
		XtSetArg(args[n], XmNlabelPixmap, p); n++;
		XtSetArg(args[n], XmNuserData, fe_image_button_names[i].data); n++;
		XtSetArg(args[n], XmNindicatorOn, FALSE); n++; /* Windows-esk */
#if 0
		/* looks ugly */
		XtSetArg(args[n], XmNmarginHeight, 0); n++;
		XtSetArg(args[n], XmNmarginWidth, 0); n++;
#endif
		children[i] = XmCreateToggleButtonGadget(parent, name, args, n);

		XtAddCallback(children[i], XmNvalueChangedCallback,
					  fe_image_align_cb, (XtPointer)w_data);
		fe_register_dependent(w_data->properties, children[i],
							  FE_MAKE_DEPENDENCY(PROP_IMAGE_ALIGN),
							  fe_image_align_update_cb, (XtPointer)w_data);
    }
	
	XtManageChildren(children, i);
}

static void
fe_make_image_page(MWContext *context, Widget parent,
				   fe_EditorImagePropertiesWidgets* w_data)
{
	Arg av [32];
	int ac;
	Widget form;
	Widget kids[32];
	Widget main_image_frame;
	Widget main_image_form;
	Widget main_image_text;
	Widget main_image_browse;
	Widget main_image_edit;
	Widget main_image_leave;
	Widget alt_frame;
	Widget alt_form;
	Widget alt_image_label;
	Widget alt_image_text;
	Widget alt_image_browse;
	Widget alt_image_edit;
	Widget alt_text_label;
	Widget alt_text_text;
	Widget align_frame;
	Widget align_form;
	Widget align_rc;
	Widget align_info;
	Widget dimensions_frame;
	Widget dimensions_form;
	Widget original_radio;
	Widget custom_radio;
	Widget height_label;
	Widget height_text;
	Widget height_pixels;
	Widget width_label;
	Widget width_text;	
	Widget width_pixels;
	Widget constrain;
	Widget space_frame;
	Widget space_form;
	Widget left_right_label;
	Widget left_right_text;
	Widget left_right_pixels;
	Widget top_bottom_label;
	Widget top_bottom_text;
	Widget top_bottom_pixels;
	Widget solid_border_label;
	Widget solid_text;
	Widget solid_pixels;
	Widget remove_image_map;
	Dimension width;
	Dimension height;
	Widget wide_guy;
	int i;
	XtCallbackRec callback;

#if 0
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	form = XmCreateForm(parent, "imageProperties", av, ac);
#else
	form = parent;
#endif

	/**********************************************************************/
	/*  Define the Image file name Frame and its form 4MAR96RCJ		*/
	/**********************************************************************/
	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	main_image_frame = fe_CreateFrame(form, "imageFile", av, ac);

	ac = 0;
	main_image_form = XmCreateForm(main_image_frame, "form", av, ac);
 
	i  = 0; /* number of children */

	/* top row: text, browse, edit */
	ac = 0;
	kids[i++] = main_image_text = fe_CreateTextField(main_image_form,
													 "imageFile",
													 av, ac);
	w_data->main_image = main_image_text;
	XtAddCallback(main_image_text, XmNvalueChangedCallback,
				  fe_image_main_image_cb,(XtPointer)w_data);

	ac = 0;
	kids[i++] = main_image_browse = XmCreatePushButtonGadget(main_image_form,
															 "browse", 
															 av, ac);
	XtAddCallback(main_image_browse, XmNactivateCallback,
				  fe_image_main_image_browse_cb, (XtPointer)w_data);

	ac = 0;
	XtSetArg(av[ac], XmNsensitive,        False);           ac++;
	kids[i++] = main_image_edit = XmCreatePushButtonGadget(main_image_form,
														   "editImage",
														   av, ac);
	XtAddCallback(main_image_edit, XmNactivateCallback,
				  fe_image_edit_image_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, main_image_edit,
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_MAIN_IMAGE),
						  fe_image_edit_image_update_cb, (XtPointer)w_data);

	/* attachments */
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget, main_image_browse); ac++;
	XtSetValues(main_image_text, av, ac);

	XtVaGetValues(main_image_text, XmNheight, &height, NULL);
  
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget, main_image_edit); ac++;
	XtSetArg(av[ac], XmNheight, height); ac++;
	XtSetValues(main_image_browse, av, ac);
  
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNheight, height); ac++;
	XtSetValues(main_image_edit, av, ac);

	/* next row: leave at original location, use as background */
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET);   ac++;
	XtSetArg(av[ac], XmNtopWidget, main_image_browse); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM);   ac++;
	kids[i++] = main_image_leave = XmCreateToggleButtonGadget(main_image_form,
															  "leaveImage",
															  av, ac);
	XtAddCallback(main_image_leave, XmNvalueChangedCallback,
				  fe_image_copy_cb, (XtPointer)w_data);

	fe_register_dependent(w_data->properties,
						  main_image_leave, 
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_COPY),
						  fe_image_copy_update_cb, (XtPointer)w_data);

#if 0
	Widget use_as_background;

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET);   ac++;
	XtSetArg(av[ac], XmNtopWidget, main_image_browse); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET);   ac++;
	XtSetArg(av[ac], XmNleftWidget, main_image_leave); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
	use_as_background = XmCreateToggleButtonGadget(main_image_form,
												   "useAsBackground",
												   av, ac);
	kids[i++] = use_as_background;

	XtAddCallback(use_as_background, XmNvalueChangedCallback,
				  fe_image_copy_cb, (XtPointer)w_data);

	fe_register_dependent(w_data->properties,
						  use_as_background, 
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_COPY),
						  fe_image_copy_update_cb, (XtPointer)w_data);
#endif
	XtManageChildren(kids, i);

	/* alternative frame */
	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtopAttachment,  XmATTACH_WIDGET);  ac++;
	XtSetArg(av[ac], XmNtopWidget,      main_image_frame);  ac++;
	alt_frame = fe_CreateFrame(form, "alternativeImage", av, ac);
	XtManageChild(alt_frame);

	ac = 0;
	alt_form = XmCreateForm(alt_frame, "form", av, ac);
	XtManageChild(alt_form);
 
	i  = 0; /* number of children */

	/* next row: alternative label, text, browse, edit */
	ac = 0;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
	kids[i++] = alt_image_label = XmCreateLabelGadget(alt_form, 
													  "alternativeImageLabel",
													  av, ac);

	ac = 0;
	kids[i++] = alt_image_text = fe_CreateTextField(alt_form,
													"alternativeImage",
													av, ac);
	w_data->alt_image = alt_image_text;
	XtAddCallback(alt_image_text, XmNvalueChangedCallback,
				  fe_image_alt_image_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, alt_image_text,
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_ALT_IMAGE),
						  fe_image_alt_image_update_cb, (XtPointer)w_data);

	ac = 0;
	kids[i++] = alt_image_browse = XmCreatePushButtonGadget(alt_form,
															"browse",
															av, ac);
	XtAddCallback(alt_image_browse, XmNactivateCallback,
				  fe_image_alt_image_browse_cb, (XtPointer)w_data);

	ac = 0;
	XtSetArg(av[ac], XmNsensitive,        False);           ac++;
	kids[i++] = alt_image_edit = XmCreatePushButtonGadget(alt_form,
														  "editImage",
														  av, ac);
	XtAddCallback(alt_image_edit, XmNactivateCallback,
				  fe_image_edit_alt_image_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, alt_image_edit,
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_ALT_IMAGE),
						  fe_image_edit_alt_image_update_cb, 
						  (XtPointer)w_data);

	/* attachments */
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, alt_image_label); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget, alt_image_browse); ac++;
	XtSetValues(alt_image_text, av, ac);

	XtVaGetValues(alt_image_text, XmNheight, &height, NULL);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM);    ac++;
	XtSetArg(av[ac], XmNheight, height); ac++;
	XtSetValues(alt_image_label, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget, alt_image_edit); ac++;
	XtSetArg(av[ac], XmNheight, height); ac++;
	XtSetValues(alt_image_browse, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNheight, height); ac++;
	XtSetValues(alt_image_edit, av, ac);

	/* next row: alternative text label, text */
	ac = 0;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
	kids[i++] = alt_text_label = XmCreateLabelGadget(alt_form, 
													 "alternativeTextLabel",
													 av, ac);
	ac = 0;
	kids[i++] = alt_text_text = fe_CreateTextField(alt_form,
												   "alternativeText",
												   av, ac);
	w_data->alt_text = alt_text_text;

	XtAddCallback(alt_text_text, XmNvalueChangedCallback,
				  fe_image_alt_text_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, alt_text_text,
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_ALT_TEXT),
						  fe_image_alt_text_update_cb, (XtPointer)w_data);

	/* attachments */
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, alt_image_text); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, alt_image_label); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
	XtSetValues(alt_text_text, av, ac);

	XtVaGetValues(alt_text_text, XmNheight, &height, NULL);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, alt_image_text); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNheight, height); ac++;
	XtSetValues(alt_text_label, av, ac);

	XtManageChildren(kids, i);

	/**********************************************************************/
	/*  Define the Alignment Frame and its form	4MAR96RCJ		*/
	/**********************************************************************/
	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM);  ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET);  ac++;
	XtSetArg(av[ac], XmNtopWidget, alt_frame);       ac++;
	align_frame = fe_CreateFrame(form, "alignment", av, ac);

	ac = 0;
	align_form = XmCreateForm(align_frame, "alignmentForm", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNorientation, XmHORIZONTAL); ac++;
	XtSetArg(av[ac], XmNradioBehavior, TRUE); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM);  ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM);  ac++;
	XtSetArg(av[ac], XmNspacing, 0);  ac++;
	align_rc = XmCreateRowColumn(align_form, "alignmentRowColumn", av, ac);
	XtManageChild(align_rc);

	fe_make_image_icons(context, align_rc, w_data);

	ac = 0;
	XtSetArg(av[ac], XmNalignment,         XmALIGNMENT_END); ac++;
	XtSetArg(av[ac], XmNrightAttachment,   XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget,     align_rc);      ac++;
#if 0
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
#endif
	align_info = XmCreateLabelGadget(align_form, "alignmentInfoLabel", av, ac);
	XtManageChild(align_info);

	/**********************************************************************/
	/*  Define the Buttons at bottom of Image Properties Tab Panel	*/
	/**********************************************************************/
	i  = 0;

	ac = 0;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNleftAttachment,  XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNsensitive,        False);           ac++;
	kids[i++] = remove_image_map = XmCreatePushButtonGadget(form,
															"removeImageMap",
															av, ac);
	XtAddCallback(remove_image_map, XmNactivateCallback,
				  fe_image_remove_imap_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, remove_image_map,
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_IMAP),
						  fe_image_remove_imap_update_cb, (XtPointer)w_data);

	XtManageChildren(kids, i);

	/**********************************************************************/
	/*  Define the Dimensions Frame and its form	4MAR96RCJ		*/
	/**********************************************************************/
	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment,   XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNtopAttachment,    XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget,        align_frame);      ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNbottomWidget,     remove_image_map);  ac++;
	dimensions_frame = fe_CreateFrame(form, "dimensions", av, ac);

	ac = 0;
	dimensions_form = XmCreateForm(dimensions_frame, 
								   "dimensionsForm", av, ac);
	i  = 0;

	ac = 0;
	kids[i++] = height_label = XmCreateLabelGadget(dimensions_form, 
												   "heightLabel",
												   av, ac);

	ac = 0;
	kids[i++] = width_label = XmCreateLabelGadget(dimensions_form, 
												  "widthLabel",
												  av, ac);

#define PROP_DIM_UPDATE (PROP_IMAGE_COPY|PROP_IMAGE_DIMENSIONS)

	ac = 0;
	XtSetArg(av[ac], XmNuserData, FALSE); ac++;
	XtSetArg(av[ac], XmNindicatorType, XmONE_OF_MANY); ac++;
	kids[i++] = original_radio = XmCreateToggleButtonGadget(dimensions_form, 
															"originalSize",
															av, ac);
	XtAddCallback(original_radio, XmNvalueChangedCallback,
				  fe_image_original_size_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, original_radio,
						  FE_MAKE_DEPENDENCY(PROP_DIM_UPDATE),
						  fe_image_original_size_update_cb, (XtPointer)w_data);

	ac = 0;
	XtSetArg(av[ac], XmNuserData, TRUE); ac++;
	XtSetArg(av[ac], XmNindicatorType, XmONE_OF_MANY); ac++;
	kids[i++] = custom_radio = XmCreateToggleButtonGadget(dimensions_form, 
														  "customSize",
														  av, ac);

	XtAddCallback(custom_radio, XmNvalueChangedCallback,
				  fe_image_custom_size_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, custom_radio,
						  FE_MAKE_DEPENDENCY(PROP_DIM_UPDATE),
						  fe_image_custom_size_update_cb, (XtPointer)w_data);

	ac = 0;
	XtSetArg(av[ac], XmNcolumns, 4); ac++;
	kids[i++] = height_text = fe_CreateTextField(dimensions_form,
												 "imageHeight",
												 av, ac);
	w_data->image_height = height_text;

	XtAddCallback(height_text, XmNvalueChangedCallback,
				  fe_image_dimensions_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, height_text,
						  FE_MAKE_DEPENDENCY(PROP_DIM_UPDATE),
						  fe_image_dimensions_height_update_cb,
						  (XtPointer)w_data);

	callback.callback = fe_image_height_pixel_percent_cb;
	callback.closure = (XtPointer)w_data;

	ac = 0;
	XtSetArg(av[ac], XmNsimpleCallback, &callback); ac++;
	XtSetArg(av[ac], XmNbuttons, fe_SimpleOptionPixelPercent); ac++;
	XtSetArg(av[ac], XmNmarginHeight, 0); ac++;
	XtSetArg(av[ac], XmNmarginWidth, 0); ac++;
	height_pixels = fe_CreateSimpleOptionMenu(dimensions_form, "heightUnits",
											  av, ac);
	fe_register_dependent(w_data->properties, height_pixels,
						  FE_MAKE_DEPENDENCY(PROP_DIM_UPDATE),
						  fe_image_height_pixel_percent_update_cb,
						  (XtPointer)w_data);
	kids[i++] = height_pixels;

	ac = 0;
	kids[i++] = width_text = fe_CreateTextField(dimensions_form,
												"imageWidth",
												av, ac);
	w_data->image_width = width_text;

	XtAddCallback(width_text, XmNvalueChangedCallback,
				  fe_image_dimensions_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, width_text,
						  FE_MAKE_DEPENDENCY(PROP_DIM_UPDATE),
						  fe_image_dimensions_width_update_cb,
						  (XtPointer)w_data);

	callback.callback = fe_image_width_pixel_percent_cb;
	callback.closure = (XtPointer)w_data;

	ac = 0;
	XtSetArg(av[ac], XmNsimpleCallback, &callback); ac++;
	XtSetArg(av[ac], XmNbuttons, fe_SimpleOptionPixelPercent); ac++;
	XtSetArg(av[ac], XmNmarginHeight, 0); ac++;
	XtSetArg(av[ac], XmNmarginWidth, 0); ac++;
	width_pixels = fe_CreateSimpleOptionMenu(dimensions_form, "widthUnits",
											 av, ac);
	fe_register_dependent(w_data->properties, width_pixels,
						  FE_MAKE_DEPENDENCY(PROP_DIM_UPDATE),
						  fe_image_width_pixel_percent_update_cb,
						  (XtPointer)w_data);
	kids[i++] = width_pixels;

	ac = 0;
	kids[i++] = constrain = XmCreateToggleButtonGadget(dimensions_form,
													   "constrain",
													   av, ac);
	XtAddCallback(constrain, XmNvalueChangedCallback,
				  fe_image_constrain_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, constrain,
						  FE_MAKE_DEPENDENCY(PROP_DIM_UPDATE),
						  fe_image_constrain_update_cb, (XtPointer)w_data);

	/* do attachments */
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetValues(original_radio, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, original_radio); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetValues(custom_radio, av, ac);

	wide_guy = XfeBiggestWidget(TRUE, kids, 2); /* the two labels */
	XtVaGetValues(wide_guy, XmNwidth, &width, 0);
  
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, custom_radio); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNwidth, width); ac++;
	XtSetValues(height_label, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, height_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, wide_guy); ac++;
	XtSetValues(height_text, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, height_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, height_text); ac++;
	XtSetValues(height_pixels, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, height_text); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNwidth, width); ac++;
	XtSetValues(width_label, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, width_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, wide_guy); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget, height_text); ac++;
	XtSetValues(width_text, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, width_label); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget, height_text); ac++;
	XtSetValues(width_pixels, av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, width_text); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	XtSetValues(constrain, av, ac);

	XtManageChildren(kids, i);

	/**********************************************************************/
	/*  Create Space Around Image Frame, Form, Contents 6MAR96RCJ		*/
	/**********************************************************************/
	ac = 0;
	i  = 0;
	XtSetArg(av[ac], XmNleftAttachment,   XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget,       dimensions_frame); ac++;
	XtSetArg(av[ac], XmNrightAttachment,  XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNtopAttachment,    XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget,        align_frame);      ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNbottomWidget,     remove_image_map);  ac++;
	space_frame = fe_CreateFrame(form, "imageSpace", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment,   XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment,  XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNtopAttachment,    XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	space_form = XmCreateForm(space_frame, 
							  "imageSpaceForm", av, ac);

	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM);    ac++;
	XtSetArg(av[ac], XmNtopAttachment,  XmATTACH_FORM);    ac++;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
	kids[i++] = left_right_label = XmCreateLabelGadget(space_form, 
													   "leftRightLabel",
													   av, ac);
	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM);   ac++;
	kids[i++] = top_bottom_label = XmCreateLabelGadget(space_form, 
													   "topBottomLabel",
													   av, ac);

	XtVaGetValues(top_bottom_label, XmNwidth,&width,NULL);
	XtVaSetValues(left_right_label, XmNwidth, width,NULL);

	ac = 0;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM);  ac++;
	XtSetArg(av[ac], XmNtopAttachment,   XmATTACH_FORM);  ac++;
	kids[i++] = left_right_pixels = XmCreateLabelGadget(space_form, 
														"pixels",
														av, ac);
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment,   XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNleftAttachment,  XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget,      left_right_label);  ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget,     left_right_pixels);   ac++;
	XtSetArg(av[ac], XmNcolumns,     3);    ac++;
	kids[i++] = left_right_text = fe_CreateTextField(space_form,
													 "spaceWidth",
													 av, ac);
	w_data->margin_width = left_right_text;

	XtAddCallback(left_right_text, XmNvalueChangedCallback,
				  fe_image_margin_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, left_right_text,
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_MARGIN_WIDTH),
						  fe_image_margin_width_update_cb, (XtPointer)w_data);

	XtVaSetValues(top_bottom_label,
				  XmNtopAttachment,  XmATTACH_WIDGET,
				  XmNtopWidget,      left_right_text,
				  NULL);
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment,   XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget,       left_right_text);   ac++;
	XtSetArg(av[ac], XmNleftAttachment,  XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget,      top_bottom_label);  ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget,     left_right_pixels);      ac++;
	XtSetArg(av[ac], XmNcolumns,     3);    ac++;
	kids[i++] = top_bottom_text = fe_CreateTextField(space_form,
													 "spaceHeight",
													 av, ac);
	w_data->margin_height = top_bottom_text;
	XtAddCallback(top_bottom_text, XmNvalueChangedCallback,
				  fe_image_margin_cb, (XtPointer)w_data);
	fe_register_dependent(w_data->properties, top_bottom_text,
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_MARGIN_HEIGHT),
						  fe_image_margin_height_update_cb, (XtPointer)w_data);

	ac = 0;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNtopAttachment,  XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget,      top_bottom_text);    ac++;
	kids[i++] = solid_border_label = XmCreateLabelGadget(space_form, 
														 "solidBorderLabel",
														 av, ac);
	ac = 0;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNtopAttachment,   XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget,       left_right_text);   ac++;
	kids[i++] = top_bottom_pixels = XmCreateLabelGadget(space_form, 
														"pixels",
														av, ac);
	ac = 0;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM);   ac++;
	XtSetArg(av[ac], XmNtopAttachment,   XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget,       top_bottom_text);    ac++;
	kids[i++] = solid_pixels = XmCreateLabelGadget(space_form, 
												   "pixels",
												   av, ac);
	ac = 0;
	XtSetArg(av[ac], XmNtopAttachment,   XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget,       top_bottom_text);    ac++;
	XtSetArg(av[ac], XmNleftAttachment,  XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNleftWidget,      top_bottom_label);  ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget,     left_right_pixels);      ac++;
	XtSetArg(av[ac], XmNcolumns,     3);    ac++;
	kids[i++] = solid_text = fe_CreateTextField(space_form,
												"spaceBorder",
												av, ac);
	w_data->margin_solid = solid_text;
	XtAddCallback(solid_text, XmNvalueChangedCallback,
				  fe_image_margin_cb, (XtPointer)w_data);
	/*
	 *    Note border is dependent on link href, as it effects the
	 *    default value.
	 */
	fe_register_dependent(w_data->properties, solid_text,
				  FE_MAKE_DEPENDENCY(PROP_LINK_HREF|PROP_IMAGE_MARGIN_BORDER),
						  fe_image_margin_border_update_cb, (XtPointer)w_data);

	XtManageChildren(kids, i);

	/*
	 *    Add this last, as other update methoda are dependent on it
	 *    having run during init.
	 */
	fe_register_dependent(w_data->properties, main_image_text,
						  FE_MAKE_DEPENDENCY(PROP_IMAGE_MAIN_IMAGE),
						  fe_image_main_image_update_cb, (XtPointer)w_data);

	/**********************************************************************/
	/*  Display form by managing all of the Manager widgets 6MAR96RCJ	*/
	/**********************************************************************/
	XtManageChild(space_form);
	XtManageChild(space_frame);
	XtManageChild(main_image_form);
	XtManageChild(main_image_frame);
	XtManageChild(align_form);
	XtManageChild(align_frame);
	XtManageChild(dimensions_form);
	XtManageChild(dimensions_frame);
	XtManageChild(form);
} /* end fe_make_image_page */

Widget
fe_EditorPropertiesDialogCreate(
								MWContext *context, 
								fe_EditorPropertiesWidgets* p_data,
								Boolean is_image
)
{
	Widget   dialog;
	Widget   form;
	Widget   tab_form;
	char*    name = (is_image)?	"imagePropertiesDialog":
		                        "textPropertiesDialog";
	
	/*
	 *    Make prompt with ok, apply, cancel, no separator.
	 */
	dialog = fe_CreatePromptDialog(context, name,
								   TRUE, TRUE, TRUE, FALSE, TRUE);

	form = XtVaCreateManagedWidget(
								   "folder",
								   xmlFolderWidgetClass, dialog,
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

	if (is_image) {
		tab_form = fe_CreateTabForm(form, "imageProperties", NULL, 0);
		fe_make_image_page(context, tab_form, p_data->image);
	} else {
		tab_form = fe_CreateTabForm(form, "characterProperties", NULL, 0);
		fe_make_character_page(context, tab_form, p_data->character);
	}

	tab_form = fe_CreateTabForm(form, "linkProperties", NULL, 0);
	fe_make_link_page(context, tab_form, p_data->link);

	tab_form = fe_CreateTabForm(form, "paragraphProperties", NULL, 0);
	fe_make_paragraph_page(context, tab_form, p_data->paragraph);

	XtManageChild(dialog);

	return form;
}

void
fe_EditorPropertiesDialogDo(MWContext* context, fe_EditorPropertiesDialogType tab_type)
{
	fe_EditorPropertiesWidgets          properties;
	fe_EditorParagraphPropertiesWidgets paragraph;
	fe_EditorLinkPropertiesWidgets      link;
	fe_EditorCharacterPropertiesWidgets character;
	fe_EditorImagePropertiesWidgets     image;
	int done;
    Widget dialog;
    Widget form;
	Widget apply_button;
	Boolean apply_sensitized;
	unsigned tab_number;
	Boolean is_image;

	is_image = fe_EditorPropertiesDialogCanDo(context, XFE_EDITOR_PROPERTIES_IMAGE);

	/*
	 *    Pick the tab.
	 */
	switch (tab_type) {
	case XFE_EDITOR_PROPERTIES_TARGET:
		fe_EditorTargetPropertiesDialogDo(context);
		return;
	case XFE_EDITOR_PROPERTIES_HTML_TAG:
		fe_EditorHtmlPropertiesDialogDo(context);
		return;
	case XFE_EDITOR_PROPERTIES_TABLE:
		fe_EditorTablePropertiesDialogDo(context, XFE_EDITOR_PROPERTIES_TABLE);
		return;
	case XFE_EDITOR_PROPERTIES_IMAGE:
		if (!is_image)
			return;
		/*FALLTHRU*/
	case XFE_EDITOR_PROPERTIES_IMAGE_INSERT:
		is_image = TRUE;
		tab_number = 0;
		break;
	case XFE_EDITOR_PROPERTIES_CHARACTER:
		is_image = FALSE;
		tab_number = 0;
		break;
	case XFE_EDITOR_PROPERTIES_LINK_INSERT:
	case XFE_EDITOR_PROPERTIES_LINK:     
		tab_number = 1;
		break;
	case XFE_EDITOR_PROPERTIES_PARAGRAPH:
		tab_number = 2;
		break;
	case XFE_EDITOR_PROPERTIES_HRULE:
		fe_EditorHorizontalRulePropertiesDialogDo(context);
		return;
	default:
		return;
	}

	memset(&properties, 0, sizeof(fe_EditorPropertiesWidgets));
	memset(&paragraph, 0, sizeof(fe_EditorParagraphPropertiesWidgets));
	memset(&link, 0, sizeof(fe_EditorLinkPropertiesWidgets));
	memset(&character, 0, sizeof(fe_EditorCharacterPropertiesWidgets));
	memset(&image, 0, sizeof(fe_EditorImagePropertiesWidgets));

	/*
	 *    I'll show you mine if you show me yours.
	 */
	link.properties = &properties;
	paragraph.properties = &properties;
	image.properties = &properties;
	character.properties = &properties;

	properties.link = &link;
	properties.paragraph = &paragraph;
	if (is_image)
		properties.image = &image;
	else 
		properties.character = &character;

	properties.context = context;
	form = fe_EditorPropertiesDialogCreate(context, &properties, is_image);
	dialog = XtParent(form);

	/*
	 *   Add a bunch of callbacks to the buttons.
	 */
	XtAddCallback(dialog, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(dialog, XmNapplyCallback, fe_hrule_apply_cb, &done);
	XtAddCallback(dialog, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(dialog, XmNdestroyCallback, fe_hrule_destroy_cb, &done);

    /*
     *    Load values.
     */
	if (is_image)
		fe_EditorImagePropertiesDialogDataGet(context, &image);
	else
		fe_EditorCharacterPropertiesDialogDataGet(context, &character);
	fe_editor_link_properties_dialog_data_init(context, &link);
	fe_EditorParagraphPropertiesDialogDataGet(context, &paragraph);

	/*
	 *    We toggle the sensitivity of the apply button on/off
	 *    depending if there are changes to apply. It would be
	 *    nice to use the depdency meahcnism, but it might get
	 *    very busy.
	 */
	apply_button = XmSelectionBoxGetChild(dialog, XmDIALOG_APPLY_BUTTON);
	XtVaSetValues(apply_button, XmNsensitive, FALSE, 0);
	apply_sensitized = FALSE;
	properties.changed = 0;

    /*
     *    Popup.
     */
    XtManageChild(form);

	XmLFolderSetActiveTab(form, tab_number, True);

	/*
     *    Wait.
     */
	fe_NukeBackingStore(dialog); /* what does this do? */

	done = XmDIALOG_NONE;
	EDT_BeginBatchChanges(context);
	while (done == XmDIALOG_NONE) {

		Boolean new_image_override = FALSE;

		fe_EventLoop();
		
		if (done == XFE_DIALOG_DESTROY_BUTTON||done == XmDIALOG_CANCEL_BUTTON)
			break;

		/*
		 *    This is a horrible crock to get around the fact
		 *    that imageinsert() moves the context away from
		 *    the image. Better solutions???
		 */
		new_image_override = (properties.image && image.new_image);
		
		if (new_image_override) {
			if (apply_sensitized == TRUE) {
				XtVaSetValues(apply_button, XmNsensitive, FALSE, 0);
				apply_sensitized = TRUE;
			}
		} else {
			if (apply_sensitized == FALSE && properties.changed != 0) {
				XtVaSetValues(apply_button, XmNsensitive, TRUE, 0);
				apply_sensitized = TRUE;
			}
		}

		if (done == XmDIALOG_APPLY_BUTTON || done == XmDIALOG_OK_BUTTON) {
			/* apply */

			if ((properties.changed & PROP_IMAGE_ALL) != 0
				&&
				!fe_editor_image_properties_validate(context, &image)) {
				done = XmDIALOG_NONE;
				continue;
			}

			if (properties.changed != 0) {
				if ((properties.changed & PROP_CHAR_ALL) != 0) 
					fe_EditorCharacterPropertiesDialogSet(context, &character);
				if ((properties.changed & PROP_IMAGE_ALL) != 0)
					fe_editor_image_properties_set(context, &image);
				if ((properties.changed & PROP_LINK_ALL) != 0)
					fe_editor_link_properties_dialog_set(context, &link);
				if ((properties.changed & PROP_PARA_ALL) != 0) 
					fe_EditorParagraphPropertiesDialogSet(context, &paragraph);
			}

			if (done == XmDIALOG_APPLY_BUTTON) {
				properties.changed = 0;
				XtVaSetValues(apply_button, XmNsensitive, FALSE, 0);
				apply_sensitized = FALSE;
				done = XmDIALOG_NONE; /* keep looping */
			}
		}
	}
	EDT_EndBatchChanges(context);

    /*
     *    Unload data.
     */
	fe_DependentListDestroy(properties.dependents);

	if (done != XFE_DIALOG_DESTROY_BUTTON)
		XtDestroyWidget(dialog);
}

void
fe_EditorSetColorsDialogDo(MWContext* context)
{
	LO_Color color;
	
	fe_EditorColorGet(context, &color);

	if (fe_ColorPicker(context, &color))
		fe_EditorColorSet(context, &color);
}

Widget
fe_CreateCombo(Widget parent, char* name, Arg* p_args, Cardinal p_n)
{
  Widget pulldown;
  Arg args[8];
  Cardinal n;
  Widget button;
  char buf[64];

  sprintf(buf, "%sMenu", name);

  n = 0;
  pulldown = fe_CreatePulldownMenu(parent, buf, args, n);

  n = 0;
  button = XmCreatePushButtonGadget(pulldown, "emptyList", args, n);
  XtManageChild(button);

  n = 0;
  XtSetArg(args[n], XmNsubMenuId, pulldown); n++;

  return fe_CreateOptionMenuNoLabel(parent, name, args, n);
}

struct fe_EditorDocumentPropertiesStruct;

typedef struct fe_EditorDocumentGeneralPropertiesStruct
{
	struct fe_EditorDocumentPropertiesStruct* properties;
	
	Widget location;
	Widget title;
	Widget author;
	Widget description;
#ifdef EDITOR_SHOW_CREATE_DATE
	Widget created;
	Widget updated;
#endif /*EDITOR_SHOW_CREATE_DATE*/
	Widget keywords;
	Widget classification;

	unsigned changed;
	
} fe_EditorDocumentGeneralPropertiesStruct;

#if 0
static LO_Color**
fe_document_appearance_color_configure(
			   fe_EditorDocumentAppearancePropertiesStruct* w_data,
			   ED_EColor  c_type,
			   LO_Color** def_color_r)
{
	LO_Color** result_addr;

	*def_color_r = &w_data->colors[c_type];

	switch (c_type) {
	case DOCUMENT_BACKGROUND_COLOR:
	  result_addr = &w_data->page_data.pColorBackground;
	  break;
	case DOCUMENT_LINK_TEXT_COLOR:
	  result_addr = &w_data->page_data.pColorLink;
	  break;
	case DOCUMENT_NORMAL_TEXT_COLOR:
	  result_addr = &w_data->page_data.pColorText;
	  break;
	case DOCUMENT_FOLLOWED_TEXT_COLOR:
	  result_addr = &w_data->page_data.pColorFollowedLink;
	  break;
	default:
	  result_addr = &w_data->page_data.pColorActiveLink;
	  break;
	}

	return result_addr;
}
#endif

#if 0

static void
fe_PreviewPanelSetColors(Widget widget,
						 MWContext* context,
						 unsigned mask,
						 Pixmap bg_pixmap,
						 LO_Color* colors)
{
    LO_Color* color;
	Pixel pixel;
	Pixel bg_pixel;
	WidgetList children;
	Cardinal num_children;
	Arg args[2];
	Cardinal n;
	int color_n;
	Boolean set_bg = FALSE;
	Boolean set_fg;

	XtVaGetValues(widget,
				  XmNchildren, &children,
				  XmNnumChildren, &num_children,
				  0);

	if ((mask & DOCUMENT_USE_CUSTOM_MASK) != 0)
	    mask = ~0; /* do everything */

	if ((mask & DOCUMENT_BACKGROUND_COLOR_MASK) != 0) {
	    color = &colors[DOCUMENT_BACKGROUND_COLOR];

		bg_pixel = fe_GetPixel(context,
							color->red,
							color->green,
							color->blue);

		XtVaSetValues(widget, XmNbackground, bg_pixel, 0);
		set_bg = TRUE;
	}

	for (color_n = 0; color_n < 4; color_n++) {
  	    n = 0;
		set_fg = FALSE;
	    if ((mask & (1<<color_n)) != 0) {
		    color = &colors[color_n];
			pixel = fe_GetPixel(context,color->red,color->green,color->blue);
			XtSetArg(args[n], XmNforeground, pixel); n++;
			set_fg = TRUE;
		}
		if (set_bg || set_fg) {
		    if (set_bg)
			    XtSetArg(args[n], XmNbackground, bg_pixel); n++;
		    XtSetValues(children[color_n], args, n);
		}
	}
}

static char* fe_PreviewPanelCreate_names[] =
{ "normal", "link", "active", "followed", 0 };

static Widget
fe_PreviewPanelCreate(Widget parent, char* name, Arg* p_args, Cardinal p_n)
{
	Widget rc;
	Widget children[4];
	Cardinal nchildren;
	Arg args[8];
	Cardinal n;
	
	XtSetArg(p_args[p_n], XmNorientation, XmVERTICAL); n++;
	rc = XmCreateRowColumn(parent, name, p_args, p_n);

	for (nchildren = 0; nchildren < 4; nchildren++) {
	  name = fe_PreviewPanelCreate_names[nchildren];
	  n = 0;
	  children[nchildren] = XmCreateLabel(rc, name, args, n);
	}

	XtManageChildren(children, nchildren);

	return rc;
}

#else

typedef struct fe_PreviewPanelStruct
{
    Pixmap     bg_pixmap;
    Pixel      bg_pixel;
    Pixel      string_pixels[4];
    XmString   strings[4];
    XmFontList fontList;
    Dimension  margin_height;
    Dimension  margin_width;
    Dimension  height;
    Dimension  width;
	Dimension  shadow_thickness;
	Dimension  highlight_thickness;
	Dimension  spacing;
} fe_PreviewPanelStruct;

static void
fe_preview_draw_string(Display* display, Drawable window,
					   Pixel bg, Pixel fg,
					   XmFontList fontList, XmString string,
					   Position x, Position y,
					   Dimension width,	Dimension height,
					   Boolean underline)
{
	XGCValues    gc_values;
	XtGCMask     gc_mask;
    GC           gc;
	XRectangle   clip_rect;
	XFontStruct* font = NULL;

	memset (&gc_values, ~0, sizeof (gc_values));
	gc_values.foreground = fg;
	gc_values.background = bg;

	_XmFontListGetDefaultFont(fontList, &font); /* must include XmP.h */

	gc_mask = GCForeground|GCBackground;

	if (font != NULL) {
		gc_values.font = font->fid;
		gc_mask |= GCFont;
	}

	gc = fe_GetGCfromDW(display, window, gc_mask, &gc_values, NULL);

	clip_rect.x = x;
	clip_rect.y = y;
	clip_rect.width = width;
	clip_rect.height = height;

	if (underline) {
	  XmStringDrawUnderline(display, window, fontList,
							string,
							gc,
							x, y, width,
							XmALIGNMENT_BEGINNING, 0, /* layout direction */
							&clip_rect, /*clip_rectangle*/
							string);
	} else {
	  XmStringDraw(display, window, fontList,
				   string,
				   gc,
				   x, y, width,
				   XmALIGNMENT_BEGINNING, 0, /* layout direction */
				   &clip_rect /*clip_rectangle*/);
	}
}
					   
static void
fe_preview_panel_paint(Display* display, Drawable window, 
					   fe_PreviewPanelStruct* stuff)
{
	Position  x;
	Position  y;
	Dimension height;
	Dimension width;
	Dimension height_delta;
	Dimension margin_height;
	XmString  string;
	Pixel     fg_pixel;
	int i;

	/* use background color instead */
	XClearArea(display, window, 0, 0, 0, 0, FALSE); /* no expose */

	for (height = stuff->height; (height % 4) != 0; height++)
		;
	height_delta = height/4;

	margin_height = stuff->highlight_thickness +
		            stuff->shadow_thickness +
		            stuff->margin_height;
	x = stuff->highlight_thickness +
		stuff->shadow_thickness +
		stuff->margin_width;
	y = margin_height;

	for (i = DOCUMENT_NORMAL_TEXT_COLOR;
		 i <= DOCUMENT_FOLLOWED_TEXT_COLOR;
		 i++) {

	    fg_pixel = stuff->string_pixels[i];
		string = stuff->strings[i];

		XmStringExtent(stuff->fontList, string, &width, &height);

		fe_preview_draw_string(display, window,
							   stuff->bg_pixel, fg_pixel,
							   stuff->fontList, string,
							   x, y, width, height,
							   (i != DOCUMENT_NORMAL_TEXT_COLOR));
		y += height + (2*margin_height) + stuff->spacing; /* note: spacing */
	}
}

static void
fe_preview_panel_expose_cb(Widget widget, XtPointer closure, XtPointer cb)
{
    XmDrawingAreaCallbackStruct* cb_data = (XmDrawingAreaCallbackStruct*)cb;
	fe_PreviewPanelStruct* stuff = 
	  (fe_PreviewPanelStruct*)fe_GetUserData(widget);

	fe_preview_panel_paint(XtDisplay(widget), cb_data->window, stuff);
}

static void
fe_PreviewPanelSetColors(Widget widget,
						 MWContext* context,
						 unsigned mask,
						 Pixmap bg_pixmap,
						 LO_Color* colors)
{
	fe_PreviewPanelStruct* stuff = 
	  (fe_PreviewPanelStruct*)fe_GetUserData(widget);
    LO_Color* color;
	Pixel pixel;
	int color_n;
	Boolean set_bg = FALSE;
	Boolean set_fg;

	set_fg = FALSE;

	if ((mask & DOCUMENT_BACKGROUND_COLOR_MASK) != 0) {
	    color = &colors[DOCUMENT_BACKGROUND_COLOR];

		pixel = fe_GetPixel(context,
							color->red,
							color->green,
							color->blue);
		if (pixel != stuff->bg_pixel) {
			stuff->bg_pixel = pixel;
			/* this will generate an expose event -> cb */
			XtVaSetValues(widget, XmNbackground, stuff->bg_pixel, 0);
			set_bg = TRUE;
		}
	}

	for (color_n = 0; color_n < 4; color_n++) {
	    if ((mask & (1<<color_n)) != 0) {
		    color = &colors[color_n];
			pixel = fe_GetPixel(context,color->red,color->green,color->blue);
			stuff->string_pixels[color_n] = pixel;
			set_fg = TRUE;
		}
	}

	if (set_fg && !set_bg) /* if we set bg, we'll get an expose anyway */
	  fe_preview_panel_paint(XtDisplay(widget), XtWindow(widget), stuff);
}

/* static */ char* fe_PreviewPanelCreate_names[] =
{ "normal", "link", "active", "followed", "background", 0 };

static XtResource fe_PreviewPanelResources [] =
{
  {
	"normalLabelString", XmCXmString, XmRXmString, sizeof(XmString),
    XtOffset(fe_PreviewPanelStruct*, strings[DOCUMENT_NORMAL_TEXT_COLOR]),
	XmRImmediate,  (XtPointer)NULL
  },
  {
	"linkLabelString", XmCXmString, XmRXmString, sizeof(XmString),
    XtOffset(fe_PreviewPanelStruct*, strings[DOCUMENT_LINK_TEXT_COLOR]),
	XmRImmediate,  (XtPointer)NULL
  },
  {
	"activeLabelString", XmCXmString, XmRXmString, sizeof(XmString),
    XtOffset(fe_PreviewPanelStruct*, strings[DOCUMENT_ACTIVE_TEXT_COLOR]),
	XmRImmediate,  (XtPointer)NULL
  },
  {
	"followedLabelString", XmCXmString, XmRXmString, sizeof(XmString),
    XtOffset(fe_PreviewPanelStruct*, strings[DOCUMENT_FOLLOWED_TEXT_COLOR]),
	XmRImmediate,  (XtPointer)NULL
  },
  {
	XmNfontList, XmCFontList, XmRFontList, sizeof(XmFontList),
    XtOffset(fe_PreviewPanelStruct*, fontList),
	XmRImmediate, (XtPointer)NULL
  },
  {
	XmNmarginHeight, XmCMarginHeight, XmRVerticalDimension, sizeof(Dimension),
    XtOffset(fe_PreviewPanelStruct*, margin_height),
	XmRImmediate, (XtPointer)2
  },
  {
    XmNmarginWidth, XmCMarginWidth, XmRHorizontalDimension, sizeof(Dimension),
    XtOffset(fe_PreviewPanelStruct*, margin_width),
	XmRImmediate, (XtPointer)2
  },
  {
     XmNhighlightThickness, XmCHighlightThickness, XmRHorizontalDimension,
     sizeof (Dimension),
     XtOffset(fe_PreviewPanelStruct*, highlight_thickness),
     XmRImmediate, (XtPointer) 0 /* makes up for frame around us */
  },
  {
	 XmNshadowThickness, XmCShadowThickness, XmRHorizontalDimension,
     sizeof (Dimension), 
     XtOffset(fe_PreviewPanelStruct*, shadow_thickness),
     XmRImmediate, (XtPointer) 0
  },
  {
	 XmNspacing, XmCSpacing, XmRVerticalDimension,
     sizeof (Dimension), 
     XtOffset(fe_PreviewPanelStruct*, spacing),
     XmRImmediate, (XtPointer)4
  }
};

/* static */ Widget
fe_PreviewPanelCreate(Widget parent, char* name, Arg* p_args, Cardinal p_n)
{
	Widget drawing_area;
	XmString xm_string;
	char* resource_name;
	Pixel bg;
	XtResource copy_resources[XtNumber(fe_PreviewPanelResources)];
	Dimension height;
	Dimension width;
	Dimension total_height;
	Dimension max_width;
	fe_PreviewPanelStruct* stuff = XtNew(fe_PreviewPanelStruct);
	int i;
	
	XtSetArg(p_args[p_n], XmNuserData, stuff); p_n++;
	drawing_area = XmCreateDrawingArea(parent, name, p_args, p_n);

	XtVaGetValues(drawing_area,
				  XmNbackground, &bg,
				  XmNheight, &stuff->height,
				  XmNwidth, &stuff->width,
				  0);

	stuff->bg_pixel = bg;

	/*
	 *    Get resources.
	 */
	memcpy(copy_resources, fe_PreviewPanelResources,
		   sizeof(fe_PreviewPanelResources));
	XtGetSubresources(drawing_area, (XtPointer)stuff,
					  name, "NsPreviewPanel",
					  copy_resources,
					  XtNumber(fe_PreviewPanelResources),
					  NULL, 0);

	max_width = 0;
	total_height = 0;

	for (i = 0; i < 4; i++) {

	  if (stuff->strings[i] == NULL) { /* not set in resources */
		  resource_name = fe_PreviewPanelResources[i].resource_name;
		  xm_string = XmStringCreateLocalized(resource_name);
		  stuff->strings[i] = xm_string;
	  }

	  XmStringExtent(stuff->fontList, stuff->strings[i], &width, &height);
	  if (width > max_width)
		  max_width = width;
	  total_height += height;

	  stuff->string_pixels[i] = bg;
	}

	max_width += 2 * (stuff->highlight_thickness +
					  stuff->shadow_thickness + stuff->margin_width);
	if (stuff->width < max_width) {
		stuff->width = max_width;
		XtVaSetValues(drawing_area, XmNwidth, stuff->width, 0);
	}

	total_height += (2 * stuff->highlight_thickness) +
		            (8 * (stuff->shadow_thickness + stuff->margin_height)) +
		            (3 * stuff->spacing);
	if (stuff->height < total_height) {
		stuff->height = total_height;
		XtVaSetValues(drawing_area, XmNheight, stuff->height, 0);
	}

	XtAddCallback(drawing_area, XmNexposeCallback, fe_preview_panel_expose_cb,
				  (XtPointer)stuff);
	XtAddCallback(drawing_area, XmNdestroyCallback, fe_destroy_cleanup_cb,
				  (XtPointer)stuff);

	return drawing_area;
}
#endif

/* static */ void
fe_document_appearance_preview_update_cb(Widget widget, XtPointer closure,
								XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	fe_DependentListCallbackStruct* cb = 
	  (fe_DependentListCallbackStruct*)cb_data;
    fe_Dependency mask = cb->mask;
	LO_Color* colors;

	if (w_data->use_custom)
	    colors = w_data->colors;
	else
	    colors = w_data->default_colors;

	if ((mask & DOCUMENT_USE_CUSTOM_MASK) != 0)
	    mask = ~0; /* do all colors */

	fe_PreviewPanelSetColors(widget,
							 w_data->context,
							 mask,
							 0, /*Pixmap bg_pixmap*/
							 colors);
}

/* static */ void
fe_document_appearance_swatch_update_cb(Widget widget, XtPointer closure,
								XtPointer cb_data)
{
	ED_EColor c_type = (ED_EColor)fe_GetUserData(widget);
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	LO_Color* default_color;
	LO_Color* colors;
	Boolean sensitive;

	if (w_data->use_custom)
	    colors = w_data->colors;
	else
	    colors = w_data->default_colors;

	default_color = &colors[c_type];

	sensitive = w_data->use_custom;

	fe_SwatchSetColor(widget, default_color);
	fe_SwatchSetSensitive(widget, sensitive);
}

static void
fe_document_appearance_color_picker_do(
					   MWContext* context,
					   Widget widget,
					   fe_EditorDocumentAppearancePropertiesStruct* w_data,
					   unsigned which)
{
	LO_Color default_color;
	LO_Color* colors;
	fe_Dependency mask;

	if (w_data->use_custom)
	    colors = w_data->colors;
	else
	    colors = w_data->default_colors;

	default_color = colors[which];

	mask = (1 << which);

	if (fe_ColorPicker(w_data->context, &default_color)) {
		w_data->colors[which] = default_color;
		w_data->changed |= mask;
	    fe_DependentListCallDependents(widget,
									   w_data->dependents,
									   mask,
									   (XtPointer)w_data);
	}
}

/* static */ void
fe_document_appearance_color_cb(Widget widget, XtPointer closure,
								XtPointer cb_data)
{
    fe_Dependency mask;
	ED_EColor c_type = (ED_EColor)fe_GetUserData(widget);
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	LO_Color default_color;
	LO_Color* colors;

	if (w_data->use_custom)
	    colors = w_data->colors;
	else
	    colors = w_data->default_colors;

	default_color = colors[c_type];

	mask = (1 << ((unsigned)c_type));

	if (fe_ColorPicker(w_data->context, &default_color)) {
		w_data->colors[c_type] = default_color;
		w_data->changed |= mask;
	    fe_DependentListCallDependents(widget,
									   w_data->dependents,
									   mask,
									   closure);
	}
}

/* static */void
fe_preview_panel_click_cb(Widget widget, XtPointer closure, XtPointer cb)
{
    XmDrawingAreaCallbackStruct* cb_data = (XmDrawingAreaCallbackStruct*)cb;
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	Dimension height;
	Dimension height_delta;
	int i;

	if (!w_data->use_custom) /* hack, hack, hack */
		return;

	if (cb_data->event->type == ButtonRelease) {

		unsigned y = cb_data->event->xbutton.y;

		XtVaGetValues(widget, XmNheight, &height, 0);

		for (; (height % 4) != 0; height++)
			;
		height_delta = height/4;

		for (i = 0 ; i < 3; i++) {
			if (y < ((i+1)*height_delta))
				break;
		}
		fe_document_appearance_color_picker_do(w_data->context,
											   widget, w_data, i);
	}
}


/* static */ void
fe_document_appearance_use_custom_update_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
    Boolean is_custom_button = (fe_GetUserData(widget) != NULL);
	Boolean set = (is_custom_button == w_data->use_custom);

	XmToggleButtonGadgetSetState(widget, set, FALSE);
}

/* static */ void
fe_document_appearance_use_custom_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
    Boolean is_custom_button = (fe_GetUserData(widget) != NULL);
	Boolean set = XmToggleButtonGadgetGetState(widget);

	w_data->use_custom = (set == is_custom_button);
	w_data->changed |= DOCUMENT_USE_CUSTOM_MASK;

	fe_DependentListCallDependents(widget,
								   w_data->dependents,
								   DOCUMENT_USE_CUSTOM_MASK,
								   closure);
}

/* static */ void
fe_document_appearance_use_image_update_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	
	XmToggleButtonGadgetSetState(widget, w_data->use_image, FALSE);
}

/* static */ void
fe_document_appearance_use_image_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	Boolean set = XmToggleButtonGadgetGetState(widget);

	w_data->use_image = set;
	w_data->changed |= DOCUMENT_USE_IMAGE_MASK;

	fe_DependentListCallDependents(widget,
								   w_data->dependents,
								   DOCUMENT_USE_IMAGE_MASK,
								   closure);
}

static void
fe_document_appearance_leave_image_cb(Widget widget, XtPointer closure,
									  XtPointer cbs)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	XmToggleButtonCallbackStruct* cb
		= (XmToggleButtonCallbackStruct*)cbs;

	w_data->leave_image = cb->set;
	w_data->changed |= DOCUMENT_USE_IMAGE_MASK;
}

static void
fe_document_appearance_leave_image_update_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	
	XmToggleButtonGadgetSetState(widget, w_data->leave_image, FALSE);
}

/* static */ void
fe_document_appearance_sensitized_update_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	Boolean sensitive;
	
	sensitive = w_data->use_custom;

	XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}

/* static */ void
fe_document_appearance_image_text_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;

	w_data->changed |= DOCUMENT_BACKGROUND_IMAGE_MASK;
}

/* static */ void
fe_document_appearance_image_text_update_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)cb_data;
	Boolean sensitive = w_data->use_image;

	if (info->caller == widget)
		return; /* don't call ourselves */
	
	if ((info->mask & DOCUMENT_BACKGROUND_IMAGE_MASK) != 0)
	  fe_set_text_field(widget, w_data->image_path,
						fe_document_appearance_image_text_cb, (XtPointer)w_data);

	fe_TextFieldSetEditable(w_data->context, widget, sensitive);
}

/* static */ void
fe_document_appearance_image_text_browse_cb(Widget widget, XtPointer closure,
									 XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	Widget image_text = (Widget)fe_GetUserData(widget);

	fe_editor_browse_file_of_text(w_data->context, image_text);
}

/* static */ void
fe_document_appearance_use_image_button_update_cb(Widget widget,
												   XtPointer closure,
												   XtPointer cb_data)
{
    fe_EditorDocumentAppearancePropertiesStruct* w_data
	  = (fe_EditorDocumentAppearancePropertiesStruct*)closure;
	Boolean sensitive = w_data->use_image;

	XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}

/* static */ void
fe_document_appearance_set(
						  MWContext* context,
						  fe_EditorDocumentAppearancePropertiesStruct* w_data)
{
    LO_Color* bg_color;
    LO_Color* normal_color;
    LO_Color* link_color;
    LO_Color* active_color;
    LO_Color* followed_color;
	char*     bg_image;
	char*     p;
	char      buf[MAXPATHLEN];
	char      docname[MAXPATHLEN];
	XP_Bool   leave_image = FALSE;

    if (w_data->use_custom) {
		bg_color = &w_data->colors[DOCUMENT_BACKGROUND_COLOR];
		normal_color = &w_data->colors[DOCUMENT_NORMAL_TEXT_COLOR];
		link_color = &w_data->colors[DOCUMENT_LINK_TEXT_COLOR];
		active_color = &w_data->colors[DOCUMENT_ACTIVE_TEXT_COLOR];
		followed_color = &w_data->colors[DOCUMENT_FOLLOWED_TEXT_COLOR];
	} else {
	    bg_color = NULL;
		normal_color = link_color = active_color = followed_color = NULL;
	}

	if (w_data->use_image) {
		bg_image = fe_TextFieldGetString(w_data->image_text);

		/*
		 *    Make sure bg image is fully qualified URL.
		 */
		if (bg_image != NULL) {
		    fe_StringTrim(bg_image);

			if ((p = strchr(bg_image, ':')) == NULL) {
			    if (bg_image[0] != '/' && !w_data->is_editor_preferences) {
				    strcpy(buf, "file:");
					fe_EditorMakeName(context, docname, sizeof(buf) - 5);
					fe_dirname(&buf[5], docname);
					strcat(buf, "/");
					strcat(buf, bg_image);
				} else {
				    sprintf(buf, "file:%s", bg_image);
				}
				XtFree(bg_image);
				bg_image = XtNewString(buf);
			}
		}

		leave_image = w_data->leave_image;

	} else {
		bg_image = NULL;
	}

	if (w_data->is_editor_preferences)
	    fe_EditorPreferencesSetColors(context,
									  bg_image,
									  bg_color,
									  normal_color,
									  link_color,
									  active_color,
									  followed_color);
	else
	    fe_EditorDocumentSetColors(context,
								   bg_image,
								   leave_image,
								   bg_color,
								   normal_color,
								   link_color,
								   active_color,
								   followed_color);

	if (bg_image)
	    XtFree(bg_image);
}

/* static */ void
fe_document_appearance_init(
						  MWContext* context,
						  fe_EditorDocumentAppearancePropertiesStruct* w_data)
{
    Boolean use_custom;
	Boolean leave_image = FALSE;
	char    bg_image[MAXPATHLEN];

	w_data->context = context;

    fe_EditorDefaultGetColors(
					  &w_data->default_colors[DOCUMENT_BACKGROUND_COLOR],
					  &w_data->default_colors[DOCUMENT_NORMAL_TEXT_COLOR],
					  &w_data->default_colors[DOCUMENT_LINK_TEXT_COLOR],
					  &w_data->default_colors[DOCUMENT_ACTIVE_TEXT_COLOR],
					  &w_data->default_colors[DOCUMENT_FOLLOWED_TEXT_COLOR]);

	if (w_data->is_editor_preferences)
  	    use_custom = fe_EditorPreferencesGetColors(context,
						bg_image,
						&w_data->colors[DOCUMENT_BACKGROUND_COLOR],
						&w_data->colors[DOCUMENT_NORMAL_TEXT_COLOR],
						&w_data->colors[DOCUMENT_LINK_TEXT_COLOR],
						&w_data->colors[DOCUMENT_ACTIVE_TEXT_COLOR],
						&w_data->colors[DOCUMENT_FOLLOWED_TEXT_COLOR]);
	else
	    use_custom = fe_EditorDocumentGetColors(context,
						bg_image,
						&leave_image,
						&w_data->colors[DOCUMENT_BACKGROUND_COLOR],
						&w_data->colors[DOCUMENT_NORMAL_TEXT_COLOR],
						&w_data->colors[DOCUMENT_LINK_TEXT_COLOR],
						&w_data->colors[DOCUMENT_ACTIVE_TEXT_COLOR],
						&w_data->colors[DOCUMENT_FOLLOWED_TEXT_COLOR]);

	w_data->use_custom = use_custom;

	if (bg_image[0] != '\0') {
	  w_data->use_image = TRUE;
	  w_data->leave_image = leave_image;
	  w_data->image_path = bg_image;
	} else {
	  w_data->use_image = FALSE;
	  w_data->leave_image = FALSE;
	  w_data->image_path = NULL;
	}

	fe_DependentListCallDependents(NULL,
								   w_data->dependents,
								   ~0,
								   (XtPointer)w_data);
    w_data->image_path = NULL; /* held in TextField */
}

Dimension
fe_get_offset_from_widest(Widget* children, Cardinal nchildren)
{
	Dimension width;
	Dimension margin_width;

	Widget fat_guy = XfeBiggestWidget(TRUE, children, nchildren);
	XtVaGetValues(fat_guy, XmNwidth, &width, 0);
	XtVaGetValues(XtParent(fat_guy), XmNmarginWidth, &margin_width, 0);

	return width + margin_width;
}

static Widget
fe_document_appearance_create(
						  MWContext* context,
						  Widget parent,
						  fe_EditorDocumentAppearancePropertiesStruct* w_data)
{
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
	Cardinal nchildren;
	Cardinal nswatch;
	Dimension width;
	int i;

	Widget background_frame;
	Widget background_form;
	Widget background_image_toggle;
	Widget background_image_text;
	Widget background_image_leave;
	Widget background_image_button;

	Widget info_label;

	n = 0;
	XtSetArg(args[n], XmNentryAlignment, XmALIGNMENT_CENTER); n++;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	main_rc = XmCreateRowColumn(parent, "appearance", args, n);
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
				  fe_document_appearance_use_custom_cb, (XtPointer)w_data);

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
				  fe_document_appearance_use_custom_cb, (XtPointer)w_data);

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
#else
#if 0
	/*
	 *    make something to break up the top row from the colro buttons.
	 */
	n = 0;
	custom_form = XmCreateSeparatorGadget(colors_rc, "separator", args, n);
	XtManageChild(custom_form);
#endif
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
					  fe_document_appearance_color_cb, (XtPointer)w_data);
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

	fat_guy = XfeBiggestWidget(TRUE, children, nchildren);
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
		foo = fe_CreateSwatchButton(custom_form, name, args, n);
		XtAddCallback(foo, XmNactivateCallback,
					  fe_document_appearance_color_cb, (XtPointer)w_data);

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
	n = 0; /* I'm tired of changing resources to get this right */
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
#if 1
	fe_DependentListAddDependent(&w_data->dependents,
								 custom_preview_rc,
								 DOCUMENT_USE_CUSTOM_MASK,
								 fe_document_appearance_sensitized_update_cb,
								 (XtPointer)w_data);
#endif
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
				  fe_document_appearance_use_image_cb, (XtPointer)w_data);
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
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	background_image_leave = XmCreateToggleButtonGadget(background_form,
														"leaveImage",
														args, n);
	children[nchildren++] = background_image_leave;
	XtAddCallback(background_image_leave, XmNvalueChangedCallback,
				  fe_document_appearance_leave_image_cb, (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
						 background_image_leave,
						 (DOCUMENT_USE_IMAGE_MASK|DOCUMENT_USE_CUSTOM_MASK),
						 fe_document_appearance_leave_image_update_cb,
						 (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNuserData, background_image_text); n++;
	background_image_button = XmCreatePushButtonGadget(background_form,
													   "browseImageFile",
													   args, n);
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
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetValues(background_image_text, args, n);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, background_image_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, background_image_button); n++;
	XtSetValues(background_image_leave, args, n);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, background_image_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetValues(background_image_button, args, n);

	XtManageChildren(children, nchildren);

	/*
	 *    Bottom line info text.
	 */
	n = 0;
	info_label = XmCreateLabelGadget(main_rc, "infoLabel", args, n);
	XtManageChild(info_label);

	return main_rc;
}

#define DOCUMENT_GENERAL_TITLE           (0x1<<0)
#define DOCUMENT_GENERAL_AUTHOR          (0x1<<1)
#define DOCUMENT_GENERAL_DESCRIPTION     (0x1<<2)
#define DOCUMENT_GENERAL_KEYWORDS        (0x1<<3)
#define DOCUMENT_GENERAL_CLASSIFICATION  (0x1<<4)

static void
fe_document_general_changed_cb(Widget widget,
							   XtPointer closure, XtPointer call_data)
{
	fe_EditorDocumentGeneralPropertiesStruct* w_data
		= (fe_EditorDocumentGeneralPropertiesStruct*)closure;
    unsigned mask = (unsigned)fe_GetUserData(widget);

	w_data->changed |= mask;
}

/*
 *	Purpose:	Condense a URL to a given length.
 *              Lifted the algorithm from winfe/popup.cpp/WFE_CondenseURL()
 */
char*
FE_CondenseURL(char* target, char* source, unsigned target_len)
{
	unsigned source_len = strlen(source);
	unsigned first_half_len;
	unsigned second_half_len;

	if (source_len < target_len) {
		strcpy(target, source);
	} else {
		first_half_len = (target_len - 3)/2;
		second_half_len = target_len - first_half_len - 4; /* -1 for NUL */

		strncpy(target, source, first_half_len);
		strcpy(&target[first_half_len], "...");
		strcat(target, &source[source_len - second_half_len]);
	}
	return target;
}

static void
fe_document_general_init(MWContext* context,
						 fe_EditorDocumentGeneralPropertiesStruct* w_data)
{
	XmString xm_string;
	char buf[40]; /* will specifiy size of maximum location text length */
	char* value;

	/* set defaults */ /* <unknown> */
	xm_string = XmStringCreateLocalized(XP_GetString(XFE_UNKNOWN));
#ifdef EDITOR_SHOW_CREATE_DATE
	XtVaSetValues(w_data->created, XmNlabelString, xm_string, 0);
	XtVaSetValues(w_data->updated, XmNlabelString, xm_string, 0);
#endif /*EDITOR_SHOW_CREATE_DATE*/
	XtVaSetValues(w_data->location, XmNlabelString, xm_string, 0);
	XmStringFree(xm_string);

	/* get the location */
	fe_EditorMakeName(context, buf, sizeof(buf));
	xm_string = XmStringCreateLocalized(buf);
	XtVaSetValues(w_data->location, XmNlabelString, xm_string, 0);
	XmStringFree(xm_string);

	/* get the title */
	if ((value = fe_EditorDocumentGetTitle(context)) != NULL) {
		fe_SetTextFieldAndCallBack(w_data->title, value);
		XP_FREE(value);
	}

	/* get the author */
	if ((value = fe_EditorDocumentGetMetaData(context, "Author")) != NULL)
		fe_SetTextFieldAndCallBack(w_data->author, value);

	/* get the description */
	if ((value = fe_EditorDocumentGetMetaData(context, "Description")))
		fe_SetTextFieldAndCallBack(w_data->description, value);

	/* get the keywords */
	if ((value = fe_EditorDocumentGetMetaData(context, "Keywords")))
		fe_SetTextFieldAndCallBack(w_data->keywords, value);

	/* get the classification */
	if ((value = fe_EditorDocumentGetMetaData(context, "Classification")))
		fe_SetTextFieldAndCallBack(w_data->classification, value);

#ifdef EDITOR_SHOW_CREATE_DATE
	/* get the created */
	if ((value = fe_EditorDocumentGetMetaData(context, "Created"))) {
		xm_string = XmStringCreateLocalized(value);
		XtVaSetValues(w_data->created, XmNlabelString, xm_string, 0);
		XmStringFree(xm_string);
	}

	/* get the updated */
	if ((value = fe_EditorDocumentGetMetaData(context, "Last-Modified"))) {
		xm_string = XmStringCreateLocalized(value);
		XtVaSetValues(w_data->updated, XmNlabelString, xm_string, 0);
		XmStringFree(xm_string);
	}
#endif /*EDITOR_SHOW_CREATE_DATE*/

	w_data->changed = 0;
}

static void
fe_document_general_set(MWContext* context,
						fe_EditorDocumentGeneralPropertiesStruct* w_data)
{
	char* value;

	/* set the title */
	if ((w_data->changed & DOCUMENT_GENERAL_TITLE) != 0) {
		value = fe_TextFieldGetString(w_data->title);
		fe_EditorDocumentSetTitle(context, value);
		XtFree(value);
	}

	/* author */
	if ((w_data->changed & DOCUMENT_GENERAL_AUTHOR) != 0) {
		value = fe_TextFieldGetString(w_data->author);
		fe_EditorDocumentSetMetaData(context, "Author", value);
		XtFree(value);
	}
	
	/* description */
	if ((w_data->changed & DOCUMENT_GENERAL_DESCRIPTION) != 0) {
		value = fe_TextFieldGetString(w_data->description);
		fe_EditorDocumentSetMetaData(context, "Description", value);
		XtFree(value);
	}
	
	/* keywords */
	if ((w_data->changed & DOCUMENT_GENERAL_KEYWORDS) != 0) {
		value = fe_TextFieldGetString(w_data->keywords);
		fe_EditorDocumentSetMetaData(context, "Keywords", value);
		XtFree(value);
	}
	
	/* classification */
	if ((w_data->changed & DOCUMENT_GENERAL_CLASSIFICATION) != 0) {
		value = fe_TextFieldGetString(w_data->classification);
		fe_EditorDocumentSetMetaData(context, "Classification", value);
		XtFree(value);
	}
	
	w_data->changed = 0;
}

static Widget
fe_document_general_create(
						  MWContext* context,
						  Widget parent,
						  fe_EditorDocumentGeneralPropertiesStruct* w_data)
{
	Widget form;
	Widget location_label;
	Widget location_text;
	Widget title_label;
	Widget title_text;
	Widget author_label;
	Widget author_text;
	Widget description_label;
	Widget description_text;
#ifdef EDITOR_SHOW_CREATE_DATE
	Widget created_label;
	Widget created_text;
	Widget updated_label;
	Widget updated_text;
	Widget fat_guy;
#endif /*EDITOR_SHOW_CREATE_DATE*/
	Widget other_frame;
	Widget other_form;
	Widget info_label;
	Widget keywords_label;
	Widget keywords_text;
	Widget classification_label;
	Widget classification_text;
	Widget children[16];
	Cardinal nchildren;
	Dimension left_offset;
	Arg    args[8];
	Cardinal n;
	Cardinal i;

	n = 0;
	form = XmCreateForm(parent, "general", args, n);

	nchildren = 0;
	n = 0;
	location_label = XmCreateLabelGadget(form, "locationLabel", args, n);
	children[nchildren++] = location_label;
	
	n = 0;
	title_label = XmCreateLabelGadget(form, "titleLabel", args, n);
	children[nchildren++] = title_label;
	
	n = 0;
	author_label = XmCreateLabelGadget(form, "authorLabel", args, n);
	children[nchildren++] = author_label;
	
	n = 0;
	description_label = XmCreateLabelGadget(form, "descriptionLabel", args, n);
	children[nchildren++] = description_label;

	left_offset = fe_get_offset_from_widest(children, nchildren);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	location_text = XmCreateLabelGadget(form, "locationText", args, n);
	children[nchildren++] = location_text;
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, children[nchildren-1]); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, location_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNuserData, DOCUMENT_GENERAL_TITLE); n++;
	title_text = fe_CreateTextField(form, "titleText", args, n);
	children[nchildren++] = title_text;

	XtAddCallback(title_text, XmNvalueChangedCallback,
				  fe_document_general_changed_cb, (XtPointer)w_data);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, children[nchildren-1]); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, location_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNuserData, DOCUMENT_GENERAL_AUTHOR); n++;
	author_text = fe_CreateTextField(form, "authorText", args, n);
	children[nchildren++] = author_text;
	
	XtAddCallback(author_text, XmNvalueChangedCallback,
				  fe_document_general_changed_cb, (XtPointer)w_data);
	
	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg(args[n], XmNrows, 2); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, children[nchildren-1]); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, location_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNuserData, DOCUMENT_GENERAL_DESCRIPTION); n++;
	description_text = fe_CreateText(form, "descriptionText", args, n);
	children[nchildren++] = description_text;

	XtAddCallback(description_text, XmNvalueChangedCallback,
				  fe_document_general_changed_cb, (XtPointer)w_data);
	
	/* go back and do label attachments */
	for (i = 0; i < 4; i++) {
		n = 0;
		if (i == 0) {
			XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
		} else {
			XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
			XtSetArg(args[n], XmNtopWidget, children[i+3]); n++;
		}
		XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
		XtSetValues(children[i], args, n);
	}

	XtManageChildren(children, nchildren);

	nchildren = 0;
#ifdef EDITOR_SHOW_CREATE_DATE
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, description_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	created_label = XmCreateLabelGadget(form, "createdLabel", args, n);
	children[nchildren++] = created_label;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, created_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	updated_label = XmCreateLabelGadget(form, "updatedLabel", args, n);
	children[nchildren++] = updated_label;

	fat_guy = XfeBiggestWidget(TRUE, children, nchildren);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, description_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, fat_guy); n++;
	created_text = XmCreateLabelGadget(form, "createdText", args, n);
	children[nchildren++] = created_text;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, created_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, fat_guy); n++;
	updated_text = XmCreateLabelGadget(form, "updatedText", args, n);
	children[nchildren++] = updated_text;
#endif /*EDITOR_SHOW_CREATE_DATE*/

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
#ifdef EDITOR_SHOW_CREATE_DATE
	XtSetArg(args[n], XmNtopWidget, updated_text); n++;
#else
	XtSetArg(args[n], XmNtopWidget, description_text); n++;
#endif /*EDITOR_SHOW_CREATE_DATE*/
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	other_frame = fe_CreateFrame(form, "otherAttributes", args, n);
	children[nchildren++] = other_frame;

	XtManageChildren(children, nchildren);
#if 1

	n = 0;
	other_form = XmCreateForm(other_frame, "form", args, n);
	XtManageChild(other_form);

	nchildren = 0;
	n = 0;
	keywords_label = XmCreateLabelGadget(other_form, "keywordsLabel", args, n);
	children[nchildren++] = keywords_label;

	n = 0;
	classification_label = XmCreateLabelGadget(other_form,
											   "classificationLabel",
											   args, n);
	children[nchildren++] = classification_label;

	left_offset = fe_get_offset_from_widest(children, nchildren);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	info_label = XmCreateLabelGadget(other_form, "infoLabel", args, n);
	children[nchildren++] = info_label;

	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg(args[n], XmNrows, 2); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, info_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, info_label); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNuserData, DOCUMENT_GENERAL_KEYWORDS); n++;
	keywords_text = fe_CreateText(other_form, "keywordsText", args, n);
	children[nchildren++] = keywords_text;

	XtAddCallback(keywords_text, XmNvalueChangedCallback,
				  fe_document_general_changed_cb, (XtPointer)w_data);
	
	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg(args[n], XmNrows, 2); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, keywords_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, info_label); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNuserData, DOCUMENT_GENERAL_CLASSIFICATION); n++;
	classification_text = fe_CreateText(other_form, "classificationText",
									   args, n);
	children[nchildren++] = classification_text;

	XtAddCallback(classification_text, XmNvalueChangedCallback,
				  fe_document_general_changed_cb, (XtPointer)w_data);
	
	/* go back set attachments for labels */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, info_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetValues(keywords_label, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, keywords_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetValues(classification_label, args, n);

	XtManageChildren(children, nchildren);
#endif
	XtManageChild(form);

	w_data->location = location_text;
	w_data->title = title_text;
	w_data->author = author_text;
	w_data->description = description_text;
#ifdef EDITOR_SHOW_CREATE_DATE
	w_data->created = created_text;
	w_data->updated = updated_text;
#endif /*EDITOR_SHOW_CREATE_DATE*/
	w_data->keywords = keywords_text;
	w_data->classification = classification_text;

	return form;
}

typedef enum
{
  XFE_USERMETA_VIEW,
  XFE_USERMETA_MODIFY,
  XFE_USERMETA_NEW
} fe_AdvancedUserState;

typedef struct fe_NameValueItemList
{
    fe_NameValueItem* items;
    unsigned          nitems;    /* logical number of items */
    unsigned          size; /* size of vector */
	Widget            widget;
	Boolean           is_dirty;
    int               selected_item;
	unsigned          mask;
} fe_NameValueItemList;

#define NOTHING_SELECTED (-1)

typedef struct fe_EditorDocumentAdvancedPropertiesStruct
{
    MWContext* context;
	fe_NameValueItemList  system_list;
	fe_NameValueItemList  user_list;
	fe_NameValueItemList* active_list;
    Boolean  item_is_new;
    Boolean  item_is_dirty;
    Widget   name_text;
    Widget   value_text;
    unsigned changed;
    fe_DependentList* dependents;
} fe_EditorDocumentAdvancedPropertiesStruct;

#define DOCUMENT_ADVANCED_HTTP_LIST (0x1<<0)
#define DOCUMENT_ADVANCED_META_LIST (0x1<<1)
#define DOCUMENT_ADVANCED_ITEM      (0x1<<2)
#define DOCUMENT_ADVANCED_SELECTION (0x1<<3)

static void
fe_document_advanced_tell_me_eh(Widget widget, XtPointer closure,
								XEvent* event, Boolean* keep_going)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	fe_NameValueItemList* my_list;

	*keep_going = TRUE;

	XmProcessTraversal(widget, XmTRAVERSE_CURRENT);

	/*
	 *    We may get called after the callback (next), so..
	 */
	if (w_data->active_list != NULL && w_data->active_list->widget == widget)
		return;

	if (w_data->user_list.widget == widget)
		my_list = &w_data->user_list;
	else
		my_list = &w_data->system_list;
		
	w_data->active_list = my_list;
	my_list->selected_item = NOTHING_SELECTED;
	my_list->is_dirty = FALSE;
	if (my_list->nitems == 0)
		w_data->item_is_new = TRUE;
	else
		w_data->item_is_new = FALSE;
	w_data->item_is_dirty = FALSE;

	fe_DependentListCallDependents(widget,
								   w_data->dependents,
								   DOCUMENT_ADVANCED_SELECTION,
								   closure);
}

static void
fe_document_advanced_list_cb(Widget widget, XtPointer closure,
									XtPointer foo)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	XmListCallbackStruct* cb_data = (XmListCallbackStruct*)foo;
	fe_NameValueItemList* my_list;
	
	if (cb_data->reason != XmCR_SINGLE_SELECT)
  	    return;

	if (w_data->user_list.widget == widget)
		my_list = &w_data->user_list;
	else
		my_list = &w_data->system_list;

	/*
	 *    For some strange and unknown reason this callback
	 *    adds one to the list position it provides.
	 */
	if (cb_data->item_position > 0 && cb_data->selected_item_count > 0) {
		my_list->selected_item = cb_data->item_position - 1;
	} else {
		my_list->selected_item = NOTHING_SELECTED;
	}

	if (my_list->nitems == 0)
		w_data->item_is_new = TRUE;
	else
		w_data->item_is_new = FALSE;

	w_data->active_list = my_list;
	my_list->is_dirty = FALSE;
	w_data->item_is_dirty = FALSE;

	fe_DependentListCallDependents(widget,
								   w_data->dependents,
								   DOCUMENT_ADVANCED_SELECTION,
								   closure);
}

static void
fe_copy_name_value_to_xm_list(Widget widget, fe_NameValueItem* list)
{
	unsigned i;
	char buf[512];
	XmString xm_string;

	XmListDeleteAllItems(widget);

	for (i = 0; list[i].name != NULL; i++) {
		strcpy(buf, list[i].name);
		strcat(buf, "=");
		strcat(buf, list[i].value);

		xm_string = XmStringCreateLocalized(buf);
		XmListAddItem(widget, xm_string, i + 1); /* MOTIFism */
		XmStringFree(xm_string);
	}
}

static void
fe_document_advanced_list_update_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	fe_NameValueItemList* my_list;
	Pixel    parent_bg;
	Pixel    select_bg;

	XtVaGetValues(XtParent(widget), XmNbackground, &parent_bg, 0);

	XmGetColors(XtScreen(widget), fe_cmap(w_data->context),
				parent_bg, NULL, NULL, NULL, &select_bg);

	if (w_data->user_list.widget == widget)
		my_list = &w_data->user_list;
	else
		my_list = &w_data->system_list;

	if (my_list->is_dirty) {
		fe_copy_name_value_to_xm_list(widget, my_list->items);
		my_list->is_dirty = FALSE;
	}

	if (w_data->active_list == my_list &&
		my_list->selected_item != NOTHING_SELECTED) {
	    XmListSelectPos(widget, my_list->selected_item + 1, FALSE);
	} else {
		XmListDeselectAllItems(widget);
	}

	if (w_data->active_list == my_list)
		XtVaSetValues(widget, XmNbackground, select_bg, 0);
	else
		XtVaSetValues(widget, XmNbackground, parent_bg, 0);
}

static void
fe_document_advanced_value_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;

	if (w_data->item_is_dirty == FALSE) {
		w_data->item_is_dirty = TRUE;

		fe_DependentListCallDependents(widget,
									   w_data->dependents,
									   DOCUMENT_ADVANCED_ITEM,
									   closure);
	}
}

static void
fe_document_advanced_name_update_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)cb_data;
	char* name;
	
	if (info->caller == widget)
	  return;
	
	if (w_data->item_is_new) {
		fe_TextFieldSetString(widget, "", FALSE);
	    fe_TextFieldSetEditable(w_data->context, widget, TRUE);
		XtVaSetValues(XtParent(widget), XmNinitialFocus, widget, 0);
	} else {
		fe_NameValueItemList* active_list = w_data->active_list;
  	    if (active_list != NULL &&
			active_list->selected_item != NOTHING_SELECTED) {
		    name = active_list->items[active_list->selected_item].name;
			
			fe_TextFieldSetString(widget, name, FALSE);
		} else {
			fe_TextFieldSetString(widget, "", FALSE);
		}
	    fe_TextFieldSetEditable(w_data->context, widget, FALSE);
	}
}

static void
fe_document_advanced_value_update_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	fe_DependentListCallbackStruct* info = 
		(fe_DependentListCallbackStruct*)cb_data;
	char* value;
	
	if (info->caller == widget)
	  return;
	
	if (w_data->item_is_new) {
	    fe_TextFieldSetString(widget, "", FALSE);
	    fe_TextFieldSetEditable(w_data->context, widget, TRUE);
	} else {
		fe_NameValueItemList* active_list = w_data->active_list;
  	    if (active_list != NULL &&
			active_list->selected_item != NOTHING_SELECTED) {
		    value = active_list->items[active_list->selected_item].value;
			
			if (value == NULL)
			  value = "";

		    fe_TextFieldSetString(widget, value, FALSE);
			fe_TextFieldSetEditable(w_data->context, widget, TRUE);
#if 0
			XmProcessTraversal(widget, XmTRAVERSE_CURRENT);
#endif
		}	else { /* nothing selected */
		    fe_TextFieldSetString(widget, "", FALSE);
			fe_TextFieldSetEditable(w_data->context, widget, FALSE);
		}
	}
}

static void
fe_document_advanced_new_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	w_data->item_is_new = TRUE;
	w_data->item_is_dirty = FALSE;

	fe_DependentListCallDependents(widget,
								   w_data->dependents,
								   DOCUMENT_ADVANCED_SELECTION,
								   closure);
}

#if 0
static void
fe_document_advanced_new_update_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	Boolean sensitive = (w_data->item_is_new == FALSE);
	
	XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}
#endif

static void
list_set_item(fe_NameValueItemList* active_list, unsigned index,
				char* name, char* value)
{
    unsigned size;

	if (active_list == NULL) /* this should be error */
		return;

    if (index == active_list->nitems)
	    active_list->nitems++;

    if (active_list->nitems > active_list->size) {

		active_list->size = active_list->nitems;
	    size = sizeof(fe_NameValueItem) * (active_list->size+1);
	    if (active_list->size == 0)
		    active_list->items = (fe_NameValueItem*)XtMalloc(size);
		else
		    active_list->items = (fe_NameValueItem*)XtRealloc((void*)active_list->items,
													 size);

		active_list->items[active_list->nitems].name = NULL;
		active_list->items[active_list->nitems].value = NULL;
	}

	if (active_list->items[index].name != NULL)
	    XtFree(active_list->items[index].name);
	if (active_list->items[index].value != NULL)
	    XtFree(active_list->items[index].value);

	active_list->items[index].name = XtNewString(name);
	if (value)
	    active_list->items[index].value = XtNewString(value);
	else
	    active_list->items[index].value = NULL;
	active_list->is_dirty = TRUE;
}

static void
list_delete_item(fe_NameValueItemList* active_list, unsigned index)
{

	if (active_list == NULL) /* this should be error */
		return;

    if (active_list->items[index].name)
	    XtFree(active_list->items[index].name);
    if (active_list->items[index].value)
	    XtFree(active_list->items[index].value);
	
	active_list->nitems--;
	if (index < active_list->nitems) {
	    memcpy(&active_list->items[index], &active_list->items[index+1],
			   (sizeof(fe_NameValueItem) * (active_list->nitems - index)));
	}
	active_list->items[active_list->nitems].name = NULL;
	active_list->items[active_list->nitems].value = NULL;
	active_list->is_dirty = TRUE;
}

static void
fe_document_advanced_delete_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	fe_NameValueItemList* active_list = w_data->active_list;

	if (active_list == NULL || active_list->selected_item == NOTHING_SELECTED)
	    return;

	list_delete_item(active_list, active_list->selected_item);
	if (active_list->selected_item > 0)
	    active_list->selected_item--;
	
	w_data->item_is_dirty = w_data->item_is_new = FALSE;
	  
	fe_DependentListCallDependents(widget,
								   w_data->dependents,
								   w_data->active_list->mask,
								   closure);
	w_data->changed |= w_data->active_list->mask;
}

static void
fe_document_advanced_delete_update_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	Boolean sensitive =
		(w_data->item_is_new == FALSE) &&
		(w_data->active_list != NULL) &&
		(w_data->active_list->selected_item != NOTHING_SELECTED);
	
	XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}

static void
fe_document_advanced_set_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	char* name;
	char* value;

	if (w_data->active_list == NULL)
	    return;

	name = fe_TextFieldGetString(w_data->name_text);
	value = fe_TextFieldGetString(w_data->value_text);

	if (w_data->item_is_new)
		w_data->active_list->selected_item = w_data->active_list->nitems;

	list_set_item(w_data->active_list, w_data->active_list->selected_item,
				  name, value);

	XtFree(name);
	XtFree(value);

	w_data->item_is_new = FALSE;
	w_data->item_is_dirty = FALSE;

	fe_DependentListCallDependents(widget,
								   w_data->dependents,
								   w_data->active_list->mask,
								   closure);
	w_data->changed |= w_data->active_list->mask;
}

static void
fe_document_advanced_set_update_cb(Widget widget, XtPointer closure,
									XtPointer cb_data)
{
    fe_EditorDocumentAdvancedPropertiesStruct* w_data = 
	  (fe_EditorDocumentAdvancedPropertiesStruct*)closure;
	Boolean sensitive = (w_data->item_is_dirty);
	
	XtVaSetValues(widget, XmNsensitive, sensitive, 0);
}

static void
fe_document_advanced_init(MWContext* context,
						  fe_EditorDocumentAdvancedPropertiesStruct* w_data)
{
	int i;
	fe_NameValueItem* list;

	list = fe_EditorDocumentGetHttpEquivMetaDataList(context);
	w_data->system_list.items = list;
	for (i = 0; list[i].name != NULL; i++)
		;
	w_data->system_list.nitems = i;
	w_data->system_list.size = i;
	w_data->system_list.is_dirty = TRUE;
	w_data->system_list.selected_item = NOTHING_SELECTED;
	w_data->system_list.mask = DOCUMENT_ADVANCED_HTTP_LIST;
	
	list = fe_EditorDocumentGetAdvancedMetaDataList(context);
	w_data->user_list.items = list;
	for (i = 0; list[i].name != NULL; i++)
		;
	w_data->user_list.nitems = i;
	w_data->user_list.size = i;
	w_data->user_list.is_dirty = TRUE;
	w_data->user_list.selected_item = NOTHING_SELECTED;
	w_data->user_list.mask = DOCUMENT_ADVANCED_META_LIST;

	fe_DependentListCallDependents(NULL,
								   w_data->dependents,
								   ~0,
								   (XtPointer)w_data);
}

static void
fe_document_advanced_set(MWContext* context,
						  fe_EditorDocumentAdvancedPropertiesStruct* w_data)
{
	if ((w_data->changed & DOCUMENT_ADVANCED_HTTP_LIST) != 0) {
		fe_EditorDocumentSetHttpEquivMetaDataList(context,
												  w_data->system_list.items);
	}

	if ((w_data->changed & DOCUMENT_ADVANCED_META_LIST) != 0) {
		fe_EditorDocumentSetAdvancedMetaDataList(context,
												 w_data->user_list.items);
	}
}

static Widget
fe_document_advanced_create(
						  MWContext* context,
						  Widget parent,
						  fe_EditorDocumentAdvancedPropertiesStruct* w_data)
{
	Widget form;
	Widget system_label;
	Widget system_list;
	Widget user_label;
	Widget user_list;
	Widget name_label;
	Widget name_text;
	Widget name_new;
	Widget name_set;
	Widget value_label;
	Widget value_text;
	Widget name_delete;
	Widget list_parent;
	Widget fat_guy;
	Dimension left_offset;
	Arg args[16];
	Cardinal n;
	Widget children[12];
	Cardinal nchildren;
	Dimension width;

#if 1
	form = parent;
#else
	n = 0;
	form = XmCreateForm(parent, "advanced", args, n);
	XtManageChild(form);
#endif

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	system_label = XmCreateLabelGadget(form, "systemLabel", args, n);
	children[nchildren++] = system_label;

#define SCROLLED_LIST_SIZE 5
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, system_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNvisibleItemCount, SCROLLED_LIST_SIZE); n++;
	XtSetArg(args[n], XmNselectionPolicy, XmSINGLE_SELECT); n++;
	XtSetArg(args[n], XmNlistSizePolicy, XmCONSTANT); n++;
	system_list = XmCreateScrolledList(form, "systemList", args, n);
	XtManageChild(system_list);
	children[nchildren++] = list_parent = XtParent(system_list);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, list_parent); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	user_label = XmCreateLabelGadget(form, "userLabel", args, n);
	children[nchildren++] = user_label;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, user_label); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNvisibleItemCount, SCROLLED_LIST_SIZE); n++;
	XtSetArg(args[n], XmNselectionPolicy, XmSINGLE_SELECT); n++;
	XtSetArg(args[n], XmNlistSizePolicy, XmCONSTANT); n++;
	user_list = XmCreateScrolledList(form, "userList", args, n);
	XtManageChild(user_list);
	children[nchildren++] = list_parent = XtParent(user_list);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, list_parent); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	name_label = XmCreateLabelGadget(form, "nameLabel", args, n);
	children[nchildren++] = name_label;

	n = 0;
	value_label = XmCreateLabelGadget(form, "valueLabel", args, n);
	children[nchildren++] = value_label;

	left_offset = fe_get_offset_from_widest(&children[nchildren-2], 2);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, list_parent); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	name_new = XmCreatePushButtonGadget(form, "new", args, n);
	children[nchildren++] = name_new;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, list_parent); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, name_new); n++;
	name_delete = XmCreatePushButtonGadget(form, "delete", args, n);
	children[nchildren++] = name_delete;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, list_parent); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, name_delete); n++;
	name_text = fe_CreateTextField(form, "nameText", args, n);
	children[nchildren++] = name_text;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, name_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetValues(value_label, args, n);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, name_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	name_set = XmCreatePushButtonGadget(form, "set", args, n);
	children[nchildren++] = name_set;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, name_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, name_set); n++;
	value_text = fe_CreateTextField(form, "valueText", args, n);
	children[nchildren++] = value_text;

	XtManageChildren(children, nchildren);

	nchildren = 0;
	children[nchildren++] = name_new;
	children[nchildren++] = name_delete;
	children[nchildren++] = name_set;
	fat_guy = XfeBiggestWidget(TRUE, children, nchildren);

	XtVaGetValues(fat_guy, XmNwidth, &width, 0);
	for (n = 0; n < nchildren; n++) {
		XtVaSetValues(children[n], XmNwidth, width, 0);
	}

	XtAddCallback(system_list, XmNsingleSelectionCallback,
				  fe_document_advanced_list_cb, (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
								 system_list,
								 (DOCUMENT_ADVANCED_SELECTION|DOCUMENT_ADVANCED_HTTP_LIST),
								 fe_document_advanced_list_update_cb,
								 (XtPointer)w_data);
	XtInsertEventHandler(system_list, ButtonPressMask, False, 
						 fe_document_advanced_tell_me_eh, (XtPointer)w_data,
						 XtListTail);

	XtAddCallback(user_list, XmNsingleSelectionCallback,
				  fe_document_advanced_list_cb, (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
								 user_list,
								 (DOCUMENT_ADVANCED_SELECTION|DOCUMENT_ADVANCED_META_LIST),
								 fe_document_advanced_list_update_cb,
								 (XtPointer)w_data);
	XtInsertEventHandler(user_list, ButtonPressMask, False, 
						 fe_document_advanced_tell_me_eh, (XtPointer)w_data,
						 XtListTail);

	XtAddCallback(name_text, XmNvalueChangedCallback,
				  fe_document_advanced_value_cb, (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
						 name_text,
						 (DOCUMENT_ADVANCED_HTTP_LIST|DOCUMENT_ADVANCED_META_LIST|DOCUMENT_ADVANCED_SELECTION),
						 fe_document_advanced_name_update_cb,
						 (XtPointer)w_data);

	XtAddCallback(value_text, XmNvalueChangedCallback,
				  fe_document_advanced_value_cb, (XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
						 value_text,
						 (DOCUMENT_ADVANCED_HTTP_LIST|DOCUMENT_ADVANCED_META_LIST|DOCUMENT_ADVANCED_SELECTION),
						 fe_document_advanced_value_update_cb,
						 (XtPointer)w_data);

	XtAddCallback(name_new, XmNactivateCallback,
				  fe_document_advanced_new_cb,(XtPointer)w_data);
#if 0
	fe_DependentListAddDependent(&w_data->dependents,
						 name_new,
						 (DOCUMENT_ADVANCED_HTTP_LIST|DOCUMENT_ADVANCED_META_LIST|DOCUMENT_ADVANCED_SELECTION),
						 fe_document_advanced_new_update_cb,
						 (XtPointer)w_data);
#endif

	XtAddCallback(name_delete, XmNactivateCallback,
				  fe_document_advanced_delete_cb,(XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
						 name_delete,
						 (DOCUMENT_ADVANCED_HTTP_LIST|DOCUMENT_ADVANCED_META_LIST|DOCUMENT_ADVANCED_SELECTION),
						 fe_document_advanced_delete_update_cb,
						 (XtPointer)w_data);

	XtAddCallback(name_set, XmNactivateCallback,
				  fe_document_advanced_set_cb,(XtPointer)w_data);
	fe_DependentListAddDependent(&w_data->dependents,
						 name_set,
						 (DOCUMENT_ADVANCED_HTTP_LIST|DOCUMENT_ADVANCED_META_LIST|DOCUMENT_ADVANCED_ITEM
						  |DOCUMENT_ADVANCED_SELECTION),
						 fe_document_advanced_set_update_cb,
						 (XtPointer)w_data);
	w_data->name_text = name_text;
	w_data->value_text = value_text;
	w_data->system_list.widget = system_list;
	w_data->user_list.widget = user_list;
	w_data->context = context;

	return form;
}

typedef struct fe_EditorDocumentPropertiesStruct
{
  MWContext* context;
  fe_EditorDocumentAppearancePropertiesStruct* appearance;
  fe_EditorDocumentGeneralPropertiesStruct*    general;
  fe_EditorDocumentAdvancedPropertiesStruct*   advanced;
} fe_EditorDocumentPropertiesStruct;

static Widget
fe_EditorDocumentPropertiesCreate(MWContext* context,
								  fe_EditorDocumentPropertiesStruct* p_data)
{
	Widget   dialog;
	Widget   form;
	Widget   tab_form;
	char*    name = "documentPropertiesDialog";
	Arg      args[8];
	Cardinal n;
	
	/*
	 *    Make prompt with ok, apply, cancel, no separator.
	 */
	dialog = fe_CreatePromptDialog(context, name,
								   TRUE, TRUE, TRUE, FALSE, TRUE);

	form = XtVaCreateManagedWidget(
								   "folder",
								   xmlFolderWidgetClass, dialog,
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

	n = 0;
	tab_form = fe_CreateTabForm(form, "appearanceProperties", args, n);
	fe_document_appearance_create(context, tab_form, p_data->appearance);
	
	tab_form = fe_CreateTabForm(form, "generalProperties", args, n);
	fe_document_general_create(context, tab_form, p_data->general);

	tab_form = fe_CreateTabForm(form, "advanced", args, n);
	fe_document_advanced_create(context, tab_form, p_data->advanced);

	XtManageChild(dialog);

	return form;
}

void
fe_EditorDocumentPropertiesDialogDo(MWContext* context, 
							  fe_EditorDocumentPropertiesDialogType tab_type)
{
	fe_EditorDocumentPropertiesStruct properties;
	fe_EditorDocumentAppearancePropertiesStruct appearance;
	fe_EditorDocumentGeneralPropertiesStruct general;
	fe_EditorDocumentAdvancedPropertiesStruct advanced;
	int done;
    Widget dialog;
    Widget form;
	Widget apply_button;
	Boolean apply_sensitized;
	unsigned tab_number;
	Boolean  three_tabs = (context->type == MWContextEditor); /* hack */

	/*
	 *    Pick the tab.
	 */
	tab_number = tab_type - 1;

	memset(&properties, 0, sizeof(fe_EditorDocumentPropertiesStruct));
	memset(&appearance, 0,
		   sizeof(fe_EditorDocumentAppearancePropertiesStruct));
	memset(&general, 0, sizeof(fe_EditorDocumentGeneralPropertiesStruct));
	memset(&advanced, 0, sizeof(fe_EditorDocumentAdvancedPropertiesStruct));

	properties.appearance = &appearance;
	properties.general = &general;
	properties.advanced = &advanced;

	properties.context = context;
	if (three_tabs) {
		form = fe_EditorDocumentPropertiesCreate(context, &properties);
		fe_document_appearance_init(context, &appearance);
		fe_document_general_init(context, &general);
		fe_document_advanced_init(context, &advanced);
		XmLFolderSetActiveTab(form, tab_number, True);
		dialog = XtParent(form);
	} else {
		/*
		 *    Make prompt with ok, apply, cancel, no separator.
		 */
		dialog = fe_CreatePromptDialog(context, "documentPropertiesDialog",
									   TRUE, TRUE, TRUE, TRUE, TRUE);

		form = XmCreateForm(dialog, "appearanceProperties", NULL, 0);
		XtManageChild(form);
		fe_document_appearance_create(context, form,  &appearance);
		XtManageChild(dialog);
		fe_document_appearance_init(context, &appearance);
		form = dialog;
	}

	/*
	 *    Initialize.
	 */
#if 0
	fe_DependentListCallDependents(NULL, properties.dependents,
								   ~0, (XtPointer)&properties);
#endif

	/*
	 *   Add a bunch of callbacks to the buttons.
	 */
	XtAddCallback(dialog, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(dialog, XmNapplyCallback, fe_hrule_apply_cb, &done);
	XtAddCallback(dialog, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(dialog, XmNdestroyCallback, fe_hrule_destroy_cb, &done);

	/*
	 *    We toggle the sensitivity of the apply button on/off
	 *    depending if there are changes to apply. It would be
	 *    nice to use the depdency meahcnism, but it might get
	 *    very busy.
	 */
	apply_button = XmSelectionBoxGetChild(dialog, XmDIALOG_APPLY_BUTTON);
	XtVaSetValues(apply_button, XmNsensitive, FALSE, 0);
	apply_sensitized = FALSE;

    /*
     *    Popup.
     */
    XtManageChild(form);

	/*
     *    Wait.
     */
	fe_NukeBackingStore(dialog); /* what does this do? */

	done = XmDIALOG_NONE;
	while (done == XmDIALOG_NONE) {

		fe_EventLoop();

		if (done == XFE_DIALOG_DESTROY_BUTTON||done == XmDIALOG_CANCEL_BUTTON)
			break;

#define SOMETHING_CHANGED() \
(appearance.changed != 0 || general.changed != 0 || advanced.changed != 0)

		if (SOMETHING_CHANGED() && apply_sensitized == FALSE) {
			XtVaSetValues(apply_button, XmNsensitive, TRUE, 0);
			apply_sensitized = TRUE;
		}

		if (done == XmDIALOG_APPLY_BUTTON || done == XmDIALOG_OK_BUTTON) {
			/* apply */

			if (SOMETHING_CHANGED()) {

				EDT_BeginBatchChanges(context);

				if (appearance.changed != 0) {
				    fe_document_appearance_set(context, &appearance);
					appearance.changed = 0;
				}
				if (general.changed != 0) {
				    fe_document_general_set(context, &general);
					general.changed = 0;
				}
				if (advanced.changed != 0) {
				    fe_document_advanced_set(context, &advanced);
					advanced.changed = 0;
				}

				EDT_EndBatchChanges(context);
			}

			if (done == XmDIALOG_APPLY_BUTTON) {
				done = XmDIALOG_NONE; /* keep looping */

				XtVaSetValues(apply_button, XmNsensitive, FALSE, 0);
				apply_sensitized = FALSE;
			}
		}
	}
#undef SOMETHING_CHANGED

    /*
     *    Unload data.
     */
	fe_DependentListDestroy(appearance.dependents);

	if (done != XFE_DIALOG_DESTROY_BUTTON)
		XtDestroyWidget(dialog);
}

typedef struct fe_EditorGeneralPreferencesStruct
{
	MWContext* context;

    Widget author;
    Widget html_editor;
    Widget image_editor;
    Widget template;
	Widget autosave_toggle;
	Widget autosave_text;

    unsigned changed;
    
} fe_EditorGeneralPreferencesStruct;

#define EDITOR_GENERAL_AUTHOR          (0x1<<0)
#define EDITOR_GENERAL_HTML_EDITOR     (0x1<<1)
#define EDITOR_GENERAL_IMAGE_EDITOR    (0x1<<2)
#define EDITOR_GENERAL_TEMPLATE        (0x1<<3)
#define EDITOR_GENERAL_AUTOSAVE        (0x1<<4)

static void
fe_general_preferences_restore_template_cb(Widget widget,
							   XtPointer closure, XtPointer call_data)
{
    char* value;
	fe_EditorGeneralPreferencesStruct* w_data
		= (fe_EditorGeneralPreferencesStruct*)closure;

	/* get the template */
	if ((value = fe_EditorDefaultGetTemplate()) == NULL)
	    value = "";

	fe_SetTextFieldAndCallBack(w_data->template, value);

	w_data->changed |= EDITOR_GENERAL_TEMPLATE;
}

static void
fe_general_preferences_changed_cb(Widget widget,
							   XtPointer closure, XtPointer call_data)
{
	fe_EditorGeneralPreferencesStruct* w_data
		= (fe_EditorGeneralPreferencesStruct*)closure;
    unsigned mask = (unsigned)fe_GetUserData(widget);

	w_data->changed |= mask;
}

static void
fe_general_preferences_autosave_toggle_cb(Widget widget,
							   XtPointer closure, XtPointer call_data)
{
	fe_EditorGeneralPreferencesStruct* w_data
		= (fe_EditorGeneralPreferencesStruct*)closure;
	XmToggleButtonCallbackStruct* cb
		= (XmToggleButtonCallbackStruct*)call_data;

	fe_TextFieldSetEditable(w_data->context, w_data->autosave_text, cb->set);
	w_data->changed |= EDITOR_GENERAL_AUTOSAVE;
}

static void
fe_general_preferences_init(MWContext* context,
						 fe_EditorGeneralPreferencesStruct* w_data)
{
	char* value;
	char* value2;
	Boolean as_enable;
	unsigned as_time;

	/* get the author */
	if ((value = fe_EditorPreferencesGetAuthor(context)) != NULL)
		fe_SetTextFieldAndCallBack(w_data->author, value);

	/* get the editors */
	fe_EditorPreferencesGetEditors(context, &value, &value2);
	if (value != NULL)
		fe_SetTextFieldAndCallBack(w_data->html_editor, value);

	if (value2 != NULL)
		fe_SetTextFieldAndCallBack(w_data->image_editor, value2);

	/* get the template */
	if ((value = fe_EditorPreferencesGetTemplate(context)))
		fe_SetTextFieldAndCallBack(w_data->template, value);

	/* get the autosave state */
	fe_EditorPreferencesGetAutoSave(context, &as_enable, &as_time);
	if (!as_enable)
		as_time = 10;
	
	fe_set_numeric_text_field(w_data->autosave_text, as_time);
	fe_TextFieldSetEditable(context, w_data->autosave_text, as_enable);
	XmToggleButtonGadgetSetState(w_data->autosave_toggle, as_enable, FALSE);

	w_data->context = context;
	w_data->changed = 0;
}

static Boolean
fe_general_preferences_validate(MWContext* context,
						fe_EditorGeneralPreferencesStruct* w_data)
{
	Boolean as_enable;
	int as_time;

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

static void
fe_general_preferences_set(MWContext* context,
						fe_EditorGeneralPreferencesStruct* w_data)
{
	char* value;
	Boolean as_enable;
	unsigned as_time;

	/* author */
	if ((w_data->changed & EDITOR_GENERAL_AUTHOR) != 0) {
		value = fe_TextFieldGetString(w_data->author);
		fe_EditorPreferencesSetAuthor(context, value);
		XtFree(value);
	}
	
	/* editors */
	value = NULL;
	if ((w_data->changed & EDITOR_GENERAL_HTML_EDITOR) != 0) {
		value = fe_TextFieldGetString(w_data->html_editor);
		fe_EditorPreferencesSetEditors(context, value, NULL);
		XtFree(value);
	}

	if ((w_data->changed & EDITOR_GENERAL_IMAGE_EDITOR) != 0) {
		value = fe_TextFieldGetString(w_data->image_editor);
		fe_EditorPreferencesSetEditors(context, NULL, value);
		XtFree(value);
	}

	/* template */
	if ((w_data->changed & EDITOR_GENERAL_TEMPLATE) != 0) {
		value = fe_TextFieldGetString(w_data->template);
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

static Widget
fe_general_preferences_create(MWContext* context, Widget parent,
							 fe_EditorGeneralPreferencesStruct* w_data)
{
    Widget main_rc;
	Widget author_frame;
	Widget author_text;
	Widget external_frame;
	Widget external_form;
	Widget html_label;
	Widget html_text;
	Widget html_browse;
	Widget image_label;
	Widget image_text;
	Widget image_browse;
	Widget template_frame;
	Widget template_form;
	Widget template_label;
	Widget template_text;
	Widget template_info_label;
	Widget template_restore;
	Widget autosave_frame;
	Widget autosave_form;
	Widget autosave_toggle;
	Widget autosave_text;
	Widget autosave_label;
	Widget children[8];
	Cardinal nchildren;
	Dimension left_offset;
	Dimension right_offset;
	Arg    args[8];
	Cardinal n;
	Cardinal i;

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	main_rc = XmCreateRowColumn(parent, "general", args, n);
	XtManageChild(main_rc);

	n = 0;
	author_frame = fe_CreateFrame(main_rc, "author", args, n);
	XtManageChild(author_frame);

	n = 0;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_AUTHOR); n++;
	author_text = fe_CreateTextField(author_frame, "authorText", args, n);
	XtManageChild(author_text);
	w_data->author = author_text;

	XtAddCallback(author_text, XmNvalueChangedCallback,
				  fe_general_preferences_changed_cb, (XtPointer)w_data);

	n = 0;
	external_frame = fe_CreateFrame(main_rc, "external", args, n);
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

	left_offset = fe_get_offset_from_widest(children, nchildren);

	n = 0;
	html_browse = XmCreatePushButtonGadget(external_form, "browse", args, n);
	children[nchildren++] = html_browse;

	n = 0;
	image_browse = XmCreatePushButtonGadget(external_form, "browse", args, n);
	children[nchildren++] = image_browse;

	right_offset = fe_get_offset_from_widest(&children[2], 2);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightOffset, right_offset); n++;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_HTML_EDITOR); n++;
	html_text = fe_CreateTextField(external_form, "htmlText", args, n);
	children[nchildren++] = html_text;
	w_data->html_editor = html_text;

	XtAddCallback(html_text, XmNvalueChangedCallback,
				  fe_general_preferences_changed_cb, (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, html_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightOffset, right_offset); n++;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_IMAGE_EDITOR); n++;
	image_text = fe_CreateTextField(external_form, "imageText", args, n);
	children[nchildren++] = image_text;
	w_data->image_editor = image_text;

	XtAddCallback(image_text, XmNvalueChangedCallback,
				  fe_general_preferences_changed_cb, (XtPointer)w_data);

	/*
	 *    Go back for browse callbacks
	 */
	XtVaSetValues(image_browse, XmNuserData, image_text, 0);
	XtAddCallback(image_browse, XmNactivateCallback,
				  fe_browse_to_text_field_cb, (XtPointer)context);

	XtVaSetValues(html_browse, XmNuserData, html_text, 0);
	XtAddCallback(html_browse, XmNactivateCallback,
				  fe_browse_to_text_field_cb, (XtPointer)context);


	/*
	 *    Go back and attach the labels and browse buttons.
	 */
	for (i = 0; i < 4; i++) {
	  n = 0;
	  if ((i & 0x1) == 0) { /* even, therefore topmost of pair */
		XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	  } else {
		XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(args[n], XmNtopWidget, html_text); n++;
	  }

	  if (i < 2) { /* label, attach to left */
		XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	  } else { /* button, attach to right */
		XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	  }
	  XtSetValues(children[i], args, n);
	}

	XtManageChildren(children, nchildren);

	n = 0;
	template_frame = fe_CreateFrame(main_rc, "template", args, n);
	XtManageChild(template_frame);

	n = 0;
	template_form = XmCreateForm(template_frame, "template", args, n);
	XtManageChild(template_form);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	template_label = XmCreateLabelGadget(template_form, "locationLabel",
										 args, n);
	children[nchildren++] = template_label;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftWidget, template_label); n++;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_TEMPLATE); n++;
	template_text = fe_CreateTextField(template_form, "templateText", args, n);
	children[nchildren++] = template_text;
	w_data->template = template_text;

	XtAddCallback(template_text, XmNvalueChangedCallback,
				  fe_general_preferences_changed_cb, (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, template_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	template_restore = XmCreatePushButtonGadget(template_form,
												"restoreDefault", args, n);
	children[nchildren++] = template_restore;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, template_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, template_restore); n++;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	template_info_label = XmCreateLabelGadget(template_form, "templateInfo",
										 args, n);
	children[nchildren++] = template_info_label;

	XtAddCallback(template_restore, XmNactivateCallback,
				  fe_general_preferences_restore_template_cb,
				  (XtPointer)w_data);

	XtManageChildren(children, nchildren);

	/*
	 *    Auto Save.
	 */
	n = 0;
	autosave_frame = fe_CreateFrame(main_rc, "autosave", args, n);
	XtManageChild(autosave_frame);

	n = 0;
	autosave_form = XmCreateForm(autosave_frame, "autosave", args, n);
	XtManageChild(autosave_form);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNindicatorType, XmN_OF_MANY); n++;
	autosave_toggle = XmCreateToggleButtonGadget(autosave_form, 
												 "autosaveEnable", args, n);
	children[nchildren++] = autosave_toggle;

	XtAddCallback(autosave_toggle, XmNvalueChangedCallback,
				  fe_general_preferences_autosave_toggle_cb,
				  (XtPointer)w_data);
	w_data->autosave_toggle = autosave_toggle;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, autosave_toggle); n++;
	XtSetArg(args[n], XmNcolumns, 4); n++;
	XtSetArg(args[n], XmNuserData, EDITOR_GENERAL_AUTOSAVE); n++;
	autosave_text = fe_CreateTextField(autosave_form, "autosaveText", args, n);
	children[nchildren++] = autosave_text;

	XtAddCallback(autosave_text, XmNvalueChangedCallback,
				  fe_general_preferences_changed_cb,
				  (XtPointer)w_data);
	w_data->autosave_text = autosave_text;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, autosave_text); n++;
	autosave_label = XmCreateLabelGadget(autosave_form,	"minutes", args, n);
	children[nchildren++] = autosave_label;

	XtManageChildren(children, nchildren);
	
	return main_rc;
}

typedef struct fe_EditorPublishPreferencesStruct
{
    Widget maintain_links;
    Widget keep_images;
    Widget publish_text;
    Widget browse_text;
    Widget username_text;
    Widget password_text;
    Widget save_password;
    unsigned changed;
} fe_EditorPublishPreferencesStruct;

#define EDITOR_PUBLISH_LINKS           (0x1<<0)
#define EDITOR_PUBLISH_IMAGES          (0x1<<1)
#define EDITOR_PUBLISH_PUBLISH         (0x1<<2)
#define EDITOR_PUBLISH_BROWSE          (0x1<<3)
#define EDITOR_PUBLISH_USERNAME        (0x1<<4)
#define EDITOR_PUBLISH_PASSWORD        (0x1<<5)
#define EDITOR_PUBLISH_PASSWORD_SAVE   (0x1<<6)

static void
fe_publish_page_changed(Widget widget, XtPointer closure, XtPointer cb)
{
	fe_EditorPublishPreferencesStruct* w_data = 
		(fe_EditorPublishPreferencesStruct*)closure;

	w_data->changed |= (unsigned)fe_GetUserData(widget);
}

static void
fe_publish_password_changed(Widget widget, XtPointer closure, XtPointer cb)
{
	fe_EditorPublishPreferencesStruct* w_data = 
		(fe_EditorPublishPreferencesStruct*)closure;

	w_data->changed |= EDITOR_PUBLISH_PASSWORD;
}

static void
fe_publish_preferences_set(MWContext* context,
						   fe_EditorPublishPreferencesStruct* w_data)
{
    char* location = NULL;
    char* browse_location = NULL;
	char* username = NULL;
	char* password = NULL;
	Boolean new_links;
	Boolean new_images;
	Boolean old_links;
	Boolean old_images;

	new_links = XmToggleButtonGetState(w_data->maintain_links);
	new_images = XmToggleButtonGetState(w_data->keep_images);

	fe_EditorPreferencesGetLinksAndImages(context, &old_links, &old_images);

	if (new_links != old_links || new_images != old_images) {
		fe_EditorPreferencesSetLinksAndImages(context, new_links, new_images);
	}

#ifdef _SECURITY_BTN_ON_PREFS
 
	/* don't need save password in prefs anymore - benjie */
	new_save_password = XmToggleButtonGetState(w_data->save_password); 

	old_save_password = fe_EditorPreferencesGetPublishLocation(context, 
															   NULL, NULL, 
															   NULL);
#endif

#define PUBLISH_MASK (EDITOR_PUBLISH_PUBLISH|  \
					  EDITOR_PUBLISH_BROWSE| \
					  EDITOR_PUBLISH_USERNAME| \
					  EDITOR_PUBLISH_PASSWORD|EDITOR_PUBLISH_PASSWORD_SAVE)

#ifdef _SECURITY_BTN_ON_PREFS		
	if (new_save_password != old_save_password || 
		(w_data->changed & PUBLISH_MASK) != 0) { 
#else
	if ((w_data->changed & PUBLISH_MASK) != 0) { 
#endif
		location = fe_TextFieldGetString(w_data->publish_text);
		browse_location = fe_TextFieldGetString(w_data->browse_text);
		username = fe_TextFieldGetString(w_data->username_text);
		password = fe_GetPasswdText(w_data->password_text);
		
		fe_EditorPreferencesSetPublishLocation(context,
											   location,
											   username,
											   password);
/*
											   new_save_password? password: 0);
*/
		fe_EditorPreferencesSetBrowseLocation(context, browse_location);
	}
#undef PUBLISH_MASK
	if (browse_location) {
	    XtFree(browse_location);
	}
	if (location) {
	    XtFree(location);
	}
	if (username) {
	    XtFree(username);
	}
	if (password) {
	    memset(password, 0, strlen(password));
		XtFree(password);
	}
}


static void
fe_publish_preferences_init(MWContext* context,
							fe_EditorPublishPreferencesStruct* w_data)
{
    char* location;
    char* browse_location;
	char* username;
	char* password;
	Boolean links;
	Boolean images;
	Boolean save_password;

	fe_EditorPreferencesGetLinksAndImages(context, &links, &images);

	save_password = fe_EditorPreferencesGetPublishLocation(context,
														   &location,
														   &username,
														   &password);

	browse_location = fe_EditorPreferencesGetBrowseLocation(context);

	XmToggleButtonSetState(w_data->maintain_links, links, FALSE);
	XmToggleButtonSetState(w_data->keep_images, images, FALSE);
	/*XmToggleButtonSetState(w_data->save_password, save_password, FALSE);*/

	if (location) {
	    fe_TextFieldSetString(w_data->publish_text, location, FALSE);

		if (username)
		    fe_TextFieldSetString(w_data->username_text, username, FALSE);
		else
		    fe_TextFieldSetString(w_data->username_text, "", FALSE);

		if (password)
		    fe_TextFieldSetString(w_data->password_text, password, FALSE);
		else
		    fe_TextFieldSetString(w_data->password_text, "", FALSE);

	}

	if (browse_location)
		fe_TextFieldSetString(w_data->browse_text, browse_location, FALSE);
	else
		fe_TextFieldSetString(w_data->browse_text, "", FALSE);

	if (browse_location) {
	    XtFree(browse_location);
	}
	if (location) {
	    XtFree(location);
	}
	if (username) {
	    XtFree(username);
	}
	if (password) {
	    memset(password, 0, strlen(password));
		XtFree(password);
	}
}

static Widget
fe_publish_preferences_create(MWContext* context, Widget parent,
							  fe_EditorPublishPreferencesStruct* w_data)
{
    Widget main_rc;

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
	Widget username_label;
	Widget username_text;
	Widget password_label;
	Widget password_text;
	Widget children[16];
	Cardinal nchildren;
	Dimension left_offset;
	Arg    args[16];
	Cardinal n;

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	main_rc = XmCreateRowColumn(parent, "publish", args, n);
	XtManageChild(main_rc);

	n = 0;
	links_frame = fe_CreateFrame(main_rc, "linksAndImages", args, n);
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
	links_sub_rc = XmCreateRowColumn(links_main_rc, "linksAndImagesToggles",
									 args, n);
	children[nchildren++] = links_sub_rc;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	links_main_tip = XmCreateLabelGadget(links_main_rc, "linksAndImagesTip",
										  args, n);
	children[nchildren++] = links_main_tip;

	XtManageChildren(children, nchildren);

	nchildren = 0;
	n = 0;
	links_toggle = XmCreateToggleButtonGadget(links_sub_rc, "linksToggle",
										  args, n);
	children[nchildren++] = links_toggle;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	links_info = XmCreateLabelGadget(links_sub_rc, "linksInfo", args, n);
	children[nchildren++] = links_info;

	n = 0;
	images_toggle = XmCreateToggleButtonGadget(links_sub_rc, "imagesToggle",
										  args, n);
	children[nchildren++] = images_toggle;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	images_info = XmCreateLabelGadget(links_sub_rc, "imagesInfo", args, n);
	children[nchildren++] = images_info;

	XtManageChildren(children, nchildren);

	n = 0;
	publish_frame = fe_CreateFrame(main_rc, "publish", args, n);
	XtManageChild(publish_frame);

	n = 0;
	publish_form = XmCreateForm(publish_frame, "publish", args, n);
	XtManageChild(publish_form);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	publish_label = XmCreateLabelGadget(publish_form, "publishLabel", args, n);
	children[nchildren++] = publish_label;

	n = 0;
	browse_label = XmCreateLabelGadget(publish_form, "browseLabel", args, n);
	children[nchildren++] = browse_label;

	left_offset = fe_get_offset_from_widest(children, 2);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNuserData, EDITOR_PUBLISH_PUBLISH); n++;
	publish_text = fe_CreateTextField(publish_form, "publishText", args, n);
	children[nchildren++] = publish_text;
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNuserData, EDITOR_PUBLISH_BROWSE); n++;
	browse_text = fe_CreateTextField(publish_form, "browseText", args, n);
	children[nchildren++] = browse_text;

	/*
	 *    Go back for browse label attachments.
	 */
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetValues(browse_label, args, n);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, browse_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	username_label = XmCreateLabelGadget(publish_form, "usernameLabel",
										 args, n);
	children[nchildren++] = username_label;

	n = 0;
	password_label = XmCreateLabelGadget(publish_form, "passwordLabel",
										 args, n);
	children[nchildren++] = password_label;

	left_offset = fe_get_offset_from_widest(&children[4], 2);

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, browse_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNuserData, EDITOR_PUBLISH_USERNAME); n++;
	username_text = fe_CreateTextField(publish_form, "usernameText", args, n);
	children[nchildren++] = username_text;
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, username_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNmaxLength, 1024); n++;
	password_text = fe_CreatePasswordField(publish_form, "passwordText",
										   args, n);
	children[nchildren++] = password_text;

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, username_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetValues(password_label, args, n);

	/* we don't need this anymore - benjie */
	/* according to the bugsplat */
#ifdef _SECURITY_BTN_ON_PREFS
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, username_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, password_text); n++;
	password_save = XmCreateToggleButtonGadget(publish_form, "savePassword",
										  args, n);
	children[nchildren++] = password_save;
#endif

	XtManageChildren(children, nchildren);

    w_data->maintain_links = links_toggle;
    w_data->keep_images = images_toggle;
    w_data->publish_text = publish_text;
    w_data->browse_text = browse_text;
    w_data->username_text = username_text;
    w_data->password_text = password_text;
#ifdef _SECURITY_BTN_ON_PREFS
    w_data->save_password = password_save; 
#endif

	XtVaSetValues(links_toggle, XmNuserData, EDITOR_PUBLISH_LINKS, 0);
	XtAddCallback(links_toggle, XmNvalueChangedCallback,
				  fe_publish_page_changed, (XtPointer)w_data);
	XtVaSetValues(images_toggle, XmNuserData, EDITOR_PUBLISH_IMAGES, 0);
	XtAddCallback(images_toggle, XmNvalueChangedCallback,
				  fe_publish_page_changed, (XtPointer)w_data);
	XtVaSetValues(publish_text, XmNuserData, EDITOR_PUBLISH_PUBLISH, 0);
	XtAddCallback(publish_text, XmNvalueChangedCallback,
				  fe_publish_page_changed, (XtPointer)w_data);
	XtVaSetValues(browse_text, XmNuserData, EDITOR_PUBLISH_BROWSE, 0);
	XtAddCallback(browse_text, XmNvalueChangedCallback,
				  fe_publish_page_changed, (XtPointer)w_data);
	XtVaSetValues(username_text, XmNuserData, EDITOR_PUBLISH_USERNAME, 0);
	XtAddCallback(username_text, XmNvalueChangedCallback,
				  fe_publish_page_changed, (XtPointer)w_data);

	XtAddCallback(password_text, XmNvalueChangedCallback,
				  fe_publish_password_changed, (XtPointer)w_data);
#ifdef _SECURITY_BTN_ON_PREFS
	XtVaSetValues(password_save, XmNuserData, EDITOR_PUBLISH_PASSWORD_SAVE, 0);
	XtAddCallback(password_save, XmNvalueChangedCallback,
				  fe_publish_page_changed, (XtPointer)w_data);
#endif

	return main_rc;
}

typedef struct fe_EditorPreferencesStruct
{
  fe_EditorGeneralPreferencesStruct*           general;
  fe_EditorDocumentAppearancePropertiesStruct* appearance;
  fe_EditorPublishPreferencesStruct*           publish;
} fe_EditorPreferencesStruct;


static Widget
fe_editor_preferences_dialog_create(MWContext* context,
								  fe_EditorPreferencesStruct* p_data)
{
	Widget   dialog;
	Widget   form;
	Widget   tab_form;
	char*    name = "editorPreferencesDialog";
	
	/*
	 *    Make prompt with ok, apply, cancel, no separator.
	 */
	dialog = fe_CreatePromptDialog(context, name,
								   TRUE, TRUE, TRUE, FALSE, TRUE);

	form = XtVaCreateManagedWidget(
								   "folder",
								   xmlFolderWidgetClass, dialog,
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

	tab_form = fe_CreateTabForm(form, "appearanceProperties", NULL, 0);
	fe_document_appearance_create(context, tab_form, p_data->appearance);
	
	tab_form = fe_CreateTabForm(form, "generalPreferences", NULL, 0);
	fe_general_preferences_create(context, tab_form, p_data->general);

	tab_form = fe_CreateTabForm(form, "publishPreferences", NULL, 0);
	fe_publish_preferences_create(context, tab_form, p_data->publish);

	XtManageChild(dialog);

	return form;
}

void
fe_EditorPreferencesDialogDo(MWContext* context, unsigned tab_type)
{
	fe_EditorPreferencesStruct properties;
	fe_EditorDocumentAppearancePropertiesStruct appearance;
	fe_EditorGeneralPreferencesStruct general;
	fe_EditorPublishPreferencesStruct publish;
	int done;
    Widget dialog;
    Widget form;
	Widget apply_button;
	Boolean apply_sensitized;
	unsigned tab_number;

	/*
	 *    Pick the tab.
	 */
	tab_number = tab_type - 1;

	memset(&properties, 0, sizeof(fe_EditorPreferencesStruct));
	memset(&appearance, 0,
		   sizeof(fe_EditorDocumentAppearancePropertiesStruct));
	memset(&general, 0, sizeof(fe_EditorGeneralPreferencesStruct));
	memset(&publish, 0, sizeof(fe_EditorPublishPreferencesStruct));

	properties.appearance = &appearance;
	properties.general = &general;
	properties.publish = &publish;

	form = fe_editor_preferences_dialog_create(context, &properties);
	dialog = XtParent(form);

	appearance.is_editor_preferences = TRUE;
	fe_document_appearance_init(context, &appearance);
	fe_general_preferences_init(context, &general);
	fe_publish_preferences_init(context, &publish);

	/*
	 *   Add a bunch of callbacks to the buttons.
	 */
	XtAddCallback(dialog, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(dialog, XmNapplyCallback, fe_hrule_apply_cb, &done);
	XtAddCallback(dialog, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(dialog, XmNdestroyCallback, fe_hrule_destroy_cb, &done);

	/*
	 *    We toggle the sensitivity of the apply button on/off
	 *    depending if there are changes to apply. It would be
	 *    nice to use the depdency meahcnism, but it might get
	 *    very busy.
	 */
	apply_button = XmSelectionBoxGetChild(dialog, XmDIALOG_APPLY_BUTTON);
	XtVaSetValues(apply_button, XmNsensitive, FALSE, 0);
	apply_sensitized = FALSE;

    /*
     *    Popup.
     */
    XtManageChild(form);

	XmLFolderSetActiveTab(form, tab_number, True);

	/*
     *    Wait.
     */
	fe_NukeBackingStore(dialog); /* what does this do? */

	done = XmDIALOG_NONE;
	while (done == XmDIALOG_NONE) {

		fe_EventLoop();

		if (done == XFE_DIALOG_DESTROY_BUTTON||done == XmDIALOG_CANCEL_BUTTON)
			break;

#define SOMETHING_CHANGED() \
(appearance.changed != 0 || general.changed != 0 || publish.changed != 0)

		if (SOMETHING_CHANGED() && apply_sensitized == FALSE) {
			XtVaSetValues(apply_button, XmNsensitive, TRUE, 0);
			apply_sensitized = TRUE;
		}

		if (done == XmDIALOG_APPLY_BUTTON || done == XmDIALOG_OK_BUTTON) {
			/* apply */

			if (SOMETHING_CHANGED()) {

				if (!fe_general_preferences_validate(context, &general)) {
					done = XmDIALOG_NONE;
					continue; /* you are not going anywhere buddy */
				}
				
				EDT_BeginBatchChanges(context);

				if (appearance.changed != 0) {
				    fe_document_appearance_set(context, &appearance);
					appearance.changed = 0;
				}
				if (general.changed != 0) {
				    fe_general_preferences_set(context, &general);
					general.changed = 0;
				}
				if (publish.changed != 0) {
				    fe_publish_preferences_set(context, &publish);
					publish.changed = 0;
				}

				EDT_EndBatchChanges(context);

				/*
				 *    Save options.
				 */
				if (!XFE_SavePrefs((char *)fe_globalData.user_prefs_file,
								   &fe_globalPrefs)) {
					fe_perror(context, XP_GetString(XFE_ERROR_SAVING_OPTIONS));
				} else {
					appearance.changed = 0;
					general.changed = 0;
					publish.changed = 0;
				}
			}

			if (done == XmDIALOG_APPLY_BUTTON) {
				done = XmDIALOG_NONE; /* keep looping */

				XtVaSetValues(apply_button, XmNsensitive, FALSE, 0);
				apply_sensitized = FALSE;
			}
		}
	}
#undef SOMETHING_CHANGED

    /*
     *    Unload data.
     */
	fe_DependentListDestroy(appearance.dependents);

	if (done != XFE_DIALOG_DESTROY_BUTTON)
		XtDestroyWidget(dialog);
}

typedef struct fe_EditorTargetPropertiesStruct
{
	Widget   text;
	Boolean  inserting;
} fe_EditorTargetPropertiesStruct;

static char*
cleanup_selection(char* target, char* source, unsigned max_size)
{
	char* p;
	char* q;
	char* end;

	for (p = source; isspace(*p); p++) /* skip beginning whitespace */
		;

	end = &p[max_size-1];
	q = target;

	while (p < end) {
		/*
		 *    Stop if we detect an unprintable, or newline.
		 */
		if (!isprint(*p) || *p == '\n' || *p == '\r')
			break;

		if (isspace(*p))
			*q++ = ' ', p++;
		else
			*q++ = *p++;
	}
	/* strip trailing whitespace */
	while (q > target && isspace(q[-1]))
		q--;
	 
	*q = '\0';

	return target;
}

static void
fe_target_properties_init(MWContext* context,
						  fe_EditorTargetPropertiesStruct* w_data)
{
	char* value;
	char buf[64];

	w_data->inserting = FALSE;
	
	if (EDT_GetCurrentElementType(context) == ED_ELEMENT_TARGET) {
		value = EDT_GetTargetData(context);
    } else {
		w_data->inserting = TRUE;

		/*
		 *    Use current selected text as suggested target name...
		 */
        if ((value = (char*)LO_GetSelectionText(context))) {
			cleanup_selection(buf, value, sizeof(buf));
			XP_FREE(value);
			value = buf;
		} else {
			value = "";
		}
	}

	/*
	 *    Zap text field.
	 */
	fe_TextFieldSetString(w_data->text, value, FALSE);
}

static char*
cleanup_string(char* target, char* source, unsigned max_size)
{
	char* p;
	char* q;
	char* end;

	if (max_size == 0)
		return NULL;

	for (p = source; isspace(*p); p++) /* skip beginning whitespace */
		;

	end = &p[max_size-1];
	q = target;

	if (strcmp(p,"")==0) return NULL;

	while (p < end) {
		/*
		 *    Stop if we detect an unprintable, or newline.
		 */
		if (*p == '\"')
			p++;
		else
			*q++ = *p++;
	}

	/* strip trailing whitespace */
	while (q > target && isspace(q[-1]))
		q--;
	 
	*q = '\0';

	return target;
}

static void
fe_target_properties_set(MWContext* context,
						 fe_EditorTargetPropertiesStruct* w_data)
{

	char* xm_value;
	char* value;
	char* target_list;
	char buf[512];
	
	xm_value = fe_TextFieldGetString(w_data->text);

	target_list = EDT_GetAllDocumentTargets(context);
	/*FIXME*/ /* look at this thing */

	value = cleanup_string(buf, xm_value, sizeof(buf));
	if (value == NULL) {
		XtFree(xm_value);
		return;
	}
		
    EDT_BeginBatchChanges(context);
	if (value[0] == '#')
		value++;
	if (w_data->inserting)
		EDT_InsertTarget(context, value);
	else
		EDT_SetTargetData(context, value);
    EDT_EndBatchChanges(context);
	
	XtFree(xm_value);
}

static Widget
fe_target_properties_create(MWContext* context, Widget form,
								   fe_EditorTargetPropertiesStruct* w_data)
{
	Widget main_rc;
	Widget label;
	Widget text;
	Arg args[8];
	Cardinal n;

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	main_rc = XmCreateRowColumn(form, "targetRC", args, n);
	XtManageChild(main_rc);

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	label = XmCreateLabelGadget(main_rc, "targetLabel", args, n);
	XtManageChild(label);

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	XtSetArg(args[n], XmNcolumns, 40); n++; /* room to move dude! */
	text = fe_CreateTextField(main_rc, "targetText", args, n);
	XtManageChild(text);

	w_data->text = text;

	return main_rc;
}

void
fe_EditorTargetPropertiesDialogDo(MWContext* context)
{
	Widget dialog;
	Widget form;
	fe_EditorTargetPropertiesStruct data;
	int done;

	/*
	 *    Make prompt with ok, no apply, cancel, separator.
	 */
	form = fe_CreatePromptDialog(context, "targetPropertiesDialog",
								 TRUE, TRUE, FALSE, TRUE, TRUE);
	dialog = XtParent(form);

	fe_target_properties_create(context, form, &data);
	fe_target_properties_init(context, &data);

	/*
	 *   Add a bunch of callbacks to the buttons.
	 */
	XtAddCallback(form, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(form, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(form, XmNdestroyCallback, fe_hrule_destroy_cb, &done);

    /*
     *    Popup.
     */
    XtManageChild(form);

	/*
     *    Wait.
     */
	fe_NukeBackingStore(dialog); /* what does this do? */

	done = XmDIALOG_NONE;
	while (done == XmDIALOG_NONE)
		fe_EventLoop();
	
	if (done == XmDIALOG_OK_BUTTON)
		fe_target_properties_set(context, &data);

	if (done != XFE_DIALOG_DESTROY_BUTTON)
		XtDestroyWidget(dialog);
}

typedef struct fe_EditorHtmlPropertiesStruct
{
	Widget   text;
	unsigned changed;
	Boolean inserting;
} fe_EditorHtmlPropertiesStruct;

static void
fe_html_properties_init(MWContext* context,
						  fe_EditorHtmlPropertiesStruct* w_data)
{
	char* value;
	char buf[64];

	w_data->inserting = FALSE;
	
	if (EDT_GetCurrentElementType(context) == ED_ELEMENT_UNKNOWN_TAG) {
		value = EDT_GetUnknownTagData(context);
    } else {
		w_data->inserting = TRUE;

		/*
		 *    Use current selected text as suggested target name...
		 */
        if ((value = (char*)LO_GetSelectionText(context))) {
			cleanup_selection(buf, value, sizeof(buf));
			XP_FREE(value);
			value = buf;
		} else {
			value = "";
		}
	}

	/*
	 *    Zap text field.
	 */
	fe_SetTextFieldAndCallBack(w_data->text, value);
	w_data->changed = 0;
}

static void
fe_html_properties_set(MWContext* context,
					   fe_EditorHtmlPropertiesStruct* w_data)
{
	char* xm_value;
	
	xm_value = fe_TextFieldGetString(w_data->text);

    EDT_BeginBatchChanges(context);
	if (EDT_GetCurrentElementType(context) == ED_ELEMENT_UNKNOWN_TAG)
		EDT_SetUnknownTagData(context, xm_value);
	else
		EDT_InsertUnknownTag(context, xm_value);
    EDT_EndBatchChanges(context);

	XtFree(xm_value);
}

Boolean
fe_html_properties_verify(MWContext* context,
						 fe_EditorHtmlPropertiesStruct* w_data)
{
	char* xm_value;
	int id = XFE_EDITOR_TAG_UNKNOWN; /* keep -O happy */
	ED_TagValidateResult e;
	Widget parent;
	
	xm_value = fe_TextFieldGetString(w_data->text);
	
	e = EDT_ValidateTag(xm_value, FALSE );

    switch (e) {
	case ED_TAG_OK:
		break;
	case ED_TAG_UNOPENED:
		id = XFE_EDITOR_TAG_UNOPENED;
		/* Unopened Tag: '<' was expected */
		break;
	case ED_TAG_UNCLOSED:
		id = XFE_EDITOR_TAG_UNCLOSED;
		/* Unopened Tag:  '>' was expected */
		break;
	case ED_TAG_UNTERMINATED_STRING:
		id = XFE_EDITOR_TAG_UNTERMINATED_STRING;
		/* Unterminated String in tag: closing quote expected */
		break;
	case ED_TAG_PREMATURE_CLOSE:
		id = XFE_EDITOR_TAG_PREMATURE_CLOSE;
		/* Premature close of tag */
		break;
	case ED_TAG_TAGNAME_EXPECTED:
		id = XFE_EDITOR_TAG_TAGNAME_EXPECTED;
		/* Tagname was expected */
		break;
	default:
		id = XFE_EDITOR_TAG_UNKNOWN;
		/* Unknown tag error */
		break;
    }

	XtFree(xm_value);

	parent = w_data->text;
		
	if (e == ED_TAG_OK) {
		return TRUE;
	} else {
		fe_error_dialog(context, w_data->text, XP_GetString(id));
		return FALSE;
	}
}

static void
fe_html_text_changed_cb(Widget widget, XtPointer closure, XtPointer cb)
{
	 fe_EditorHtmlPropertiesStruct* w_data =
		 (fe_EditorHtmlPropertiesStruct*)closure;
	 w_data->changed = 0x1;
}

static Widget
fe_html_properties_create(MWContext* context, Widget form,
								   fe_EditorHtmlPropertiesStruct* w_data)
{
	Widget main_rc;
	Widget label;
	Widget text;
	Arg args[8];
	Cardinal n;

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	main_rc = XmCreateRowColumn(form, "htmlRC", args, n);
	XtManageChild(main_rc);

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	label = XmCreateLabelGadget(main_rc, "htmlPropertiesInfo", args, n);
	XtManageChild(label);

	n = 0;
	XtSetArg(args[n], XmNeditMode, XmMULTI_LINE_EDIT); n++;
	XtSetArg(args[n], XmNcolumns, 70); n++;
	XtSetArg(args[n], XmNrows, 8); n++;
	text = XmCreateScrolledText(main_rc, "htmlText", args, n);
	fe_HackTextTranslations(text);
	XtAddCallback(text, XmNvalueChangedCallback, fe_html_text_changed_cb,
				  (XtPointer)w_data);
	XtManageChild(text);

	w_data->text = text;

	return main_rc;
}

void
fe_EditorHtmlPropertiesDialogDo(MWContext* context)
{
	Widget dialog;
	Widget form;
	Widget ok_button;
	fe_EditorHtmlPropertiesStruct data;
	int    done;

	/*
	 *    Make prompt with ok, no apply, cancel, separator.
	 */
	form = fe_CreatePromptDialog(context, "htmlPropertiesDialog",
								 TRUE, TRUE, TRUE, TRUE, TRUE);
	dialog = XtParent(form);

	fe_html_properties_create(context, form, &data);
	fe_html_properties_init(context, &data);

	/*
	 *   Add a bunch of callbacks to the buttons.
	 */
	XtAddCallback(form, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(form, XmNapplyCallback, fe_hrule_apply_cb, &done);
	XtAddCallback(form, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(form, XmNdestroyCallback, fe_hrule_destroy_cb, &done);

	ok_button = XmSelectionBoxGetChild(form, XmDIALOG_OK_BUTTON);

    /*
     *    Popup.
     */
    XtManageChild(form);

	/*
     *    Wait.
     */
	fe_NukeBackingStore(dialog); /* what does this do? */

	done = XmDIALOG_NONE;
	while (done == XmDIALOG_NONE) {
		fe_EventLoop();

		if (done == XmDIALOG_APPLY_BUTTON || done == XmDIALOG_OK_BUTTON) {
			if (fe_html_properties_verify(context, &data) == FALSE) {
				done = XmDIALOG_NONE;
			} else if (done == XmDIALOG_APPLY_BUTTON) { /* but verified */
				/* Tag seems ok */
				fe_message_dialog(context,
								  dialog,
								  XP_GetString(XFE_EDITOR_TAG_OK));
				done = XmDIALOG_NONE;
			}
		}
	}
	
	if (done == XmDIALOG_OK_BUTTON && data.changed != 0)
		fe_html_properties_set(context, &data);

	if (done != XFE_DIALOG_DESTROY_BUTTON)
		XtDestroyWidget(dialog);
}

char* fe_SimpleTableAlignment[3] = {
	"left",
	"center",
	"right"
};

char* fe_SimpleOptionAboveBelow[2] = {
	"above",
	"below"
};

char* fe_SimpleOptionPixelPercent[2] = {
	"pixels",
	"percent"
};

char* fe_SimpleOptionHorizontalAlignment[4] = {
	"default",
	"left",
	"center",
	"right"
};

char* fe_SimpleOptionVerticalAlignment[5] = {
	"default",
	"top",
	"center",
	"bottom",
	"baselines"
};

Widget
fe_CreateSimpleOptionMenu(Widget parent, char* name, Arg* p_args, Cardinal p_n)
{
	Widget pulldown;
	Widget button;
	Widget option_menu;
	Widget history_widget = NULL;
	char namebuf[64];
	Arg args[32];
	Cardinal n;
	Cardinal i;
	char** button_names = NULL;
	unsigned button_count = 0;
	unsigned button_set = 0;
	char* button_name;
	XtCallbackRec* callback = NULL;

	strcpy(namebuf, name);
	strcat(namebuf, "Menu");

	n = 0;
	pulldown = fe_CreatePulldownMenu(parent, namebuf, args, n);

	n = 0;
	for (i = 0; i < p_n; i++) {
		if (p_args[i].name == XmNsimpleCallback)
			callback = (XtCallbackRec*)p_args[i].value;
		else if (p_args[i].name == XmNbuttons)
			button_names = (char**)p_args[i].value;
		else if (p_args[i].name == XmNbuttonCount)
			button_count = (unsigned)p_args[i].value;
		else if (p_args[i].name == XmNbuttonSet)
			button_set = (unsigned)p_args[i].value;
		else
			args[n++] = p_args[i];
	}

	if (button_names == fe_SimpleOptionAboveBelow)
		button_count = XtNumber(fe_SimpleOptionAboveBelow);
	else if (button_names == fe_SimpleOptionPixelPercent)
		button_count = XtNumber(fe_SimpleOptionPixelPercent);
	else if (button_names == fe_SimpleOptionHorizontalAlignment)
		button_count = XtNumber(fe_SimpleOptionHorizontalAlignment);
	else if (button_names == fe_SimpleOptionVerticalAlignment)
		button_count = XtNumber(fe_SimpleOptionVerticalAlignment);

	for (i = 0; i < button_count; i++) {
		if (button_names) {
			button_name = button_names[i];
		} else {
			sprintf(namebuf, "button%d", i);
			button_name = namebuf;
		}
		button = XmCreatePushButtonGadget(pulldown, button_name, NULL, 0);
		XtManageChild(button);
		if (i == button_set)
			history_widget = button;

		if (callback != NULL) {
			XtAddCallback(button, XmNactivateCallback,
						  callback->callback, callback->closure);
		}
	}

	XtSetArg(args[n], XmNsubMenuId, pulldown); n++;
	option_menu = fe_CreateOptionMenu(parent, name, args, n);
	fe_UnmanageChild_safe(XmOptionLabelGadget(option_menu));

	if (history_widget)
		XtVaSetValues(option_menu, XmNmenuHistory, history_widget, 0);

	return option_menu;
}

#endif  /* EDITOR */


#ifdef EDITOR

Widget
fe_CreateSimpleRadioGroup(Widget parent, char* name, Arg* p_args, Cardinal p_n)
{
	Widget button;
	Widget option_menu;
	char namebuf[64];
	Arg args[32];
	Cardinal n;
	Cardinal i;
	char** button_names = NULL;
	unsigned button_count = 0;
	unsigned button_set = 0;
	char* button_name;

	strcpy(namebuf, name);
	strcat(namebuf, "Radio");

	n = 0;
	for (i = 0; i < p_n; i++) {
		if (p_args[i].name == XmNbuttons)
			button_names = (char**)p_args[i].value;
		else if (p_args[i].name == XmNbuttonCount)
			button_count = (unsigned)p_args[i].value;
		else if (p_args[i].name == XmNbuttonSet)
			button_set = (unsigned)p_args[i].value;
		else
			args[n++] = p_args[i];
	}

	XtSetArg(args[n], XmNradioBehavior, TRUE); n++;
	option_menu = XmCreateRowColumn(parent, name, args, n);

	if (button_names == fe_SimpleOptionAboveBelow)
		button_count = XtNumber(fe_SimpleOptionAboveBelow);
	else if (button_names == fe_SimpleOptionPixelPercent)
		button_count = XtNumber(fe_SimpleOptionPixelPercent);
	else if (button_names == fe_SimpleOptionHorizontalAlignment)
		button_count = XtNumber(fe_SimpleOptionHorizontalAlignment);
	else if (button_names == fe_SimpleOptionVerticalAlignment)
		button_count = XtNumber(fe_SimpleOptionVerticalAlignment);
	else if (button_names == fe_SimpleTableAlignment)
		button_count = XtNumber(fe_SimpleTableAlignment);

	for (i = 0; i < button_count; i++) {
		if (button_names) {
			button_name = button_names[i];
		} else {
			sprintf(namebuf, "button%d", i);
			button_name = namebuf;
		}
		n = 0;
		XtSetArg(args[n], XmNset, (i == button_set)); n++;
		XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
		button = XmCreateToggleButtonGadget(option_menu, button_name, args, n);
		XtManageChild(button);
	}

	return option_menu;
}

void
fe_SimpleRadioGroupSetWhich(Widget widget, unsigned which)
{
	Widget* children;
	Cardinal num_children;
	Cardinal i;

	XtVaGetValues(widget,
				  XmNchildren, &children,
				  XmNnumChildren, &num_children, 0);

	if (which < num_children) {

		for (i = 0; i < num_children; i++) {
			XmToggleButtonGadgetSetState(children[i], (i == which), FALSE);
		}
	}
}

void
fe_SimpleRadioGroupSetSensitive(Widget widget, Boolean sensitive)
{
	Widget* children;
	Cardinal num_children;
	Cardinal i;

	XtVaGetValues(widget,
				  XmNchildren, &children,
				  XmNnumChildren, &num_children, 0);

	for (i = 0; i < num_children; i++) {
		XtVaSetValues(children[i], XmNsensitive, sensitive, 0);
	}
}

int
fe_SimpleRadioGroupGetWhich(Widget widget)
{
	Widget* children;
	Cardinal num_children;
	Cardinal i;

	XtVaGetValues(widget,
				  XmNchildren, &children,
				  XmNnumChildren, &num_children, 0);

	for (i = 0; i < num_children; i++) {
		if (XmToggleButtonGadgetGetState(children[i]) == TRUE)
			return i;
	}
	return -1; /* ?? */
}

Widget
fe_SimpleRadioGroupGetChild(Widget widget, unsigned n)
{
	Widget*  children;
	Cardinal num_children;

	XtVaGetValues(widget,
				  XmNchildren, &children,
				  XmNnumChildren, &num_children, 0);

	if (n < num_children)
		return children[n];
	else
		return NULL;
}


unsigned
fe_ED_Alignment_to_index(ED_Alignment type)
{
    switch (type) {
	case ED_ALIGN_LEFT:
    case ED_ALIGN_ABSTOP:
	case ED_ALIGN_TOP:       return 1;
	case ED_ALIGN_CENTER:
	case ED_ALIGN_ABSCENTER: return 2;
	case ED_ALIGN_RIGHT:
	case ED_ALIGN_BOTTOM:
    case ED_ALIGN_ABSBOTTOM: return 3;
    case ED_ALIGN_BASELINE:  return 4;
	case ED_ALIGN_DEFAULT:
	default:                 return 0;
  }
}

#if 0
static void
check_children(Widget* children, Cardinal nchildren)
{
	Widget widget;
	Widget parent;
	int i;

	for (i = 0; i < nchildren; i++) {
		widget = children[i];
		parent = XtParent(widget);
		fprintf(real_stderr, "parent(%s) = %s, ", XtName(widget), 
				XtName(parent));
	}
	fprintf(real_stderr, "\n");
	
}
#endif

#define RGB_TO(r,g,b) (((r)<<16)+((g)<<8)+(b))
#define R_FROM(l)     (((l)>>16)&0xff)
#define G_FROM(l)     (((l)>>8)&0xff)
#define B_FROM(l)     ((l)&0xff)

void
fe_NewSwatchSetColor(Widget widget, LO_Color* color)
{
	unsigned long pack_color = 0;

	if (color)
		pack_color = RGB_TO(color->red, color->green, color->blue);

	XtVaSetValues(widget,
				  XmNuserData, pack_color,
				  0);
	fe_SwatchSetColor(widget, color);
}

LO_Color*
fe_NewSwatchGetColor(Widget widget, LO_Color* color)
{
	unsigned long pack_color;

	XtVaGetValues(widget, XmNuserData, &pack_color, 0);

	color->red = R_FROM(pack_color);
	color->green = G_FROM(pack_color);
	color->blue = B_FROM(pack_color);

	return color;
}

void
fe_bg_group_use_color_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	XmToggleButtonCallbackStruct* cbs = (XmToggleButtonCallbackStruct*)cbd;
	Widget swatch = (Widget)closure;
	
	fe_WidgetSetSensitive(swatch, cbs->set);
}

void
fe_bg_group_swatch_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	MWContext* context = fe_WidgetToMWContext(widget);
	LO_Color   color;
	Widget     toggle = (Widget)closure;

	fe_NewSwatchGetColor(widget, &color);

	if (fe_ColorPicker(context, &color)) {
		fe_NewSwatchSetColor(widget, &color);
		XmToggleButtonSetState(toggle, TRUE, FALSE);
	}
}

void
fe_bg_use_image_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	XmToggleButtonCallbackStruct* cbs = (XmToggleButtonCallbackStruct*)cbd;
	MWContext* context = fe_WidgetToMWContext(widget);
	Widget     text_widget = (Widget)closure;

	fe_TextFieldSetEditable(context, text_widget, cbs->set);
}

void
fe_bg_image_browse_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	MWContext* context = fe_WidgetToMWContext(widget);
	Widget     text_widget = (Widget)closure;

	fe_url_browse_file_of_text(context, text_widget);
}

static void
fe_cancel_button_set_cancel(Widget widget, Boolean can_cancel)
{
    char* name;
	char* string;
	XmString xm_string;

	if (can_cancel)
	    name = "cancelLabelString";
	else
	    name = "closeLabelString";

	string = XfeSubResourceGetWidgetStringValue(widget, name, XmCXmString);
	if (!string)
	  string = name;
	xm_string = XmStringCreateLocalized(string);

	XtVaSetValues(widget, XmNlabelString, xm_string, 0);

	XmStringFree(xm_string);
}

typedef struct fe_PublishDialogStruct
{
    MWContext* context;
    Widget local_publish_info;
    Widget local_include_images;
    Widget local_include_files;
    Widget local_list;
    Widget local_select_all;
    Widget local_select_none;
    Widget publish_combo;
    Widget publish_location_text;
    Widget publish_username_text;
    Widget publish_password_text;
    Widget publish_save_password;
    Widget publish_use_default;
	Widget title_text;
	char*  url_pathname;
    char** files;
} fe_PublishDialogStruct;

static void
fe_publish_set_xmlist_from_data(fe_PublishDialogStruct* w_data)
{
	unsigned n;
	XmString xm_string;
	char*    s;
	char*    tmp;

	XmListDeleteAllItems(w_data->local_list);

	if (w_data->files == NULL)
		return;

	for (n = 0; w_data->files[n] != NULL; n++) {

		s = w_data->files[n];
		tmp = NULL;

#if 0
		dirlen = strlen(w_data->directory);

		/* windows does not clean this up, let's see how we go */
		if (strncmp(s, w_data->directory, dirlen) == 0 && s[dirlen] == '/') {
		    tmp = XP_STRDUP(&s[dirlen+1]);
			s = tmp;
		}
#endif

		xm_string = XmStringCreateLocalized(s);
	    XmListAddItem(w_data->local_list, xm_string, n+1);
		XmStringFree(xm_string);
		if (tmp != NULL)
			XP_FREE(tmp);
	}
}

static void
fe_publish_include_select_all(fe_PublishDialogStruct* w_data)
{
	unsigned n;
	unsigned nitems;

	XtVaGetValues(w_data->local_list, XmNitemCount, &nitems, 0);

	for (n = 0; n < nitems; n++)
	    XmListSelectPos(w_data->local_list, n+1, FALSE);
}

static void
free_string_array(char** vector)
{
    unsigned n;
	if (vector) {
	    for (n = 0; vector[n] != NULL; n++)
		    XP_FREE(vector[n]);
		XP_FREE(vector);
	}
}

static void
fe_publish_set_include_all_files(fe_PublishDialogStruct* w_data)
{
    MWContext* context = w_data->context;
    char**     all_files;
	unsigned   nfiles;
	char* local_filename;
	char* p;

	free_string_array(w_data->files);

	XP_ConvertUrlToLocalFile(w_data->url_pathname, &local_filename);

	if (!local_filename) /* oops */
		return;

	/* get the directory name */
	p = XP_STRRCHR(local_filename, '/');
	if (p != NULL && p != local_filename)
		*p = '\0';
	 
	all_files = NET_AssembleAllFilesInDirectory(context, local_filename);

	nfiles = 0;
	if (all_files != NULL) {
		for (; all_files[nfiles] != NULL; nfiles++) {
			p = all_files[nfiles];
			if (*p == '/') { /* absolute */
				p = (char*)XP_ALLOC(XP_STRLEN(p) + 6);
				XP_STRCPY(p, "file:");
				XP_STRCAT(p, all_files[nfiles]);
				XP_FREE(all_files[nfiles]);
				all_files[nfiles] = p;
			}
		}
	}

	if (nfiles > 0) {
		w_data->files = all_files;
	} else {
		w_data->files = NULL;
	}

	fe_publish_set_xmlist_from_data(w_data);
	fe_publish_include_select_all(w_data);

	XmToggleButtonGadgetSetState(w_data->local_include_images, FALSE, FALSE);
	XmToggleButtonGadgetSetState(w_data->local_include_files, TRUE, FALSE);

	XP_FREE(local_filename);
}

static void
fe_publish_set_include_image_files(fe_PublishDialogStruct* w_data)
{
    MWContext* context = w_data->context;
	unsigned   len;
	char*      s;
	unsigned   n;
	unsigned   nfiles;
    char*      filenames;
	XP_Bool*   selected = NULL;
	Boolean    links_p;

	free_string_array(w_data->files);

	fe_EditorPreferencesGetLinksAndImages(context, NULL, &links_p);

	filenames = EDT_GetAllDocumentFilesSelected(context, &selected, links_p);

	nfiles = 0;
	if (filenames != NULL && selected != NULL) {
	    for (s = filenames; (len = XP_STRLEN(s)) != 0; s += len + 1)
			nfiles++;
	}
	
	if (nfiles > 0) {
		w_data->files = (char**)XP_ALLOC((nfiles+1)*sizeof(char*));

	    for (n = 0, s = filenames; *s != '\0'; n++, s += XP_STRLEN(s) + 1) {
			w_data->files[n] = XP_STRDUP(s);
		}
		w_data->files[n] = NULL;

	} else {
		w_data->files = NULL;
	}
	XP_FREEIF(filenames);

	fe_publish_set_xmlist_from_data(w_data);

	/* Files that are selected by default. */
	for (n = 0; n < nfiles; n++) {
	  if (selected[n]) {
	    XmListSelectPos(w_data->local_list, n+1, FALSE);
	  }
	}
	XP_FREEIF(selected);

	/*
	 *    Gray out if there are no images in doc.
	 */
#if 0
	XtVaSetValues(w_data->local_include_images, XmNsensitive, (nfiles > 0), 0);
#endif
	XmToggleButtonGadgetSetState(w_data->local_include_images, TRUE, FALSE);
	XmToggleButtonGadgetSetState(w_data->local_include_files, FALSE, FALSE);
}

static void
fe_publish_include_image_files_cb(Widget widget, XtPointer closure,
								  XtPointer cb_data)
{
	XmToggleButtonCallbackStruct* info = 
		(XmToggleButtonCallbackStruct*)cb_data;
    fe_PublishDialogStruct* w_data = (fe_PublishDialogStruct*)closure;

	if (!info->set)
		return;

	fe_publish_set_include_image_files(w_data);
}

static void
fe_publish_include_all_files_cb(Widget widget, XtPointer closure,
								  XtPointer cb_data)
{
	XmToggleButtonCallbackStruct* info = 
		(XmToggleButtonCallbackStruct*)cb_data;
    fe_PublishDialogStruct* w_data = (fe_PublishDialogStruct*)closure;

	if (!info->set)
		return;

	fe_publish_set_include_all_files(w_data);
}

static void
fe_publish_include_select_none_cb(Widget widget, XtPointer closure,
								  XtPointer cb_data)
{
    fe_PublishDialogStruct* w_data = (fe_PublishDialogStruct*)closure;

	XmListDeselectAllItems(w_data->local_list);
}

static void
fe_publish_include_select_all_cb(Widget widget, XtPointer closure,
								  XtPointer cb_data)
{
    fe_PublishDialogStruct* w_data = (fe_PublishDialogStruct*)closure;
	fe_publish_include_select_all(w_data);
}

static void
fe_publish_set_lup(fe_PublishDialogStruct* w_data,
				   char* location, char* username, char* password)
{
	if (location) {
	    fe_TextFieldSetString(w_data->publish_location_text, location, FALSE);
		
		if (username) {
		    fe_TextFieldSetString(w_data->publish_username_text,
								  username, FALSE);
		} else {
		    fe_TextFieldSetString(w_data->publish_username_text, "", FALSE);
		}
		
		if (password) {
		    fe_TextFieldSetString(w_data->publish_password_text, password,
								  FALSE);
			XtVaSetValues(w_data->publish_save_password, XmNset, TRUE, 0);
		} else {
		    fe_TextFieldSetString(w_data->publish_password_text, "", FALSE);
			XtVaSetValues(w_data->publish_save_password, XmNset, FALSE, 0);
		}
	}
}

static void
fe_publish_combo_selection_cb(Widget w, XtPointer closure, XtPointer cb_data)
{
    fe_PublishDialogStruct* w_data = (fe_PublishDialogStruct*)closure;
	DtComboBoxCallbackStruct* info = (DtComboBoxCallbackStruct*)cb_data;
	char* location;
	char* username;
	char* password;

	if (EDT_GetPublishingHistory(info->item_position,
								 &location, &username, &password)) {
#if 0
		/* combo callback has already screwed us */
		char* target_file;
		char* last;
		int   len;

		/*
		 *    If the new location is a directory, try to append the
		 *    basename of the existing target onto the new location.
		 */
		target_file = fe_TextFieldGetString(w_data->publish_location_text);
		if ((last = XP_STRRCHR(target_file, '/')) != NULL)
			last++;
		else
			last = target_file;

		len = XP_STRLEN(location);
		if (last != NULL && len > 0 && location[len-1] == '/') {
			char* tmp;
			len = len + XP_STRLEN(location) + 1;
			tmp = (char*)XP_ALLOC(len);
			XP_STRCPY(tmp, location);
			XP_STRCAT(tmp, last);
			XP_FREE(location);
			location = tmp;
		}
		XtFree(target_file);
#endif

		fe_publish_set_lup(w_data, location, username, password);
		if (location != NULL)
			XP_FREE(location);
		if (username != NULL)
			XP_FREE(username);
		if (password != NULL) {
			memset(password, 0, strlen(password));
			XP_FREE(password);
		}
	}
}

static void
fe_publish_use_default_cb(Widget widget, XtPointer closure,
								  XtPointer cb_data)
{
    fe_PublishDialogStruct* w_data = (fe_PublishDialogStruct*)closure;
	char* location;
	char* username;
	char* password;

	fe_EditorPreferencesGetPublishLocation(w_data->context,
										   &location,
										   &username,
										   &password);

	if (location) {
		fe_publish_set_lup(w_data, location, username, password);
		if (location != NULL)
			XP_FREE(location);
		if (username != NULL)
			XP_FREE(username);
		if (password != NULL) {
			memset(password, 0, strlen(password));
			XP_FREE(password);
		}
	}
}

static void
fe_publish_dialog_init(MWContext* context, fe_PublishDialogStruct* w_data)
{
	XmString xm_string;
	History_entry* hist_ent;
	char  buf[MAXPATHLEN];
	char* location_dir;
	char* location_file;
	char* location;
	char* username;
	char* password;
	char* title;
	Boolean has_default;
	Boolean save_password;
	int n;

	w_data->url_pathname = NULL;

	/* get the location */
	hist_ent = SHIST_GetCurrent(&context->hist);
	if(hist_ent && hist_ent->address) {

		w_data->url_pathname = XP_STRDUP(hist_ent->address);

		FE_CondenseURL(buf, w_data->url_pathname, 40);

		xm_string = XmStringCreateLtoR(buf,XmFONTLIST_DEFAULT_TAG);
		XtVaSetValues(w_data->local_publish_info,
					  XmNlabelString, xm_string, 0);
		XmStringFree(xm_string);
    }

	fe_publish_set_include_image_files(w_data);

	location_file = username = password = NULL; /* settle over-zealous BE */
	location_dir = EDT_GetDefaultPublishURL(context,
											&location_file,
											&username,
											&password);

	/*
	 *    Set the title
	 */
	if (!(title = fe_EditorDocumentGetTitle(context)))
		title = EDT_GetPageTitleFromFilename(location_file);

	if (title != NULL) {
		fe_TextFieldSetString(w_data->title_text, title, False);
		XP_FREE(title);
	} else {
		fe_TextFieldSetString(w_data->title_text, "", False);
	}
	
	XtVaSetValues(w_data->local_include_files, XmNsensitive,
				  (NET_IsLocalFileURL(w_data->url_pathname)), 0);

	/*
	 *    Set the history list first, as comobo will always callback
	 *    on text field and set the location - we don't always want that.
	 */
	for (n = 0; EDT_GetPublishingHistory(n, &location, 0, 0); n++) {
		xm_string = XmStringCreateLocalized(location);
		DtComboBoxAddItem(w_data->publish_combo, xm_string, n + 1, FALSE);
		XmStringFree(xm_string);
		XP_FREE(location);
	}

	if (n > 0) {
		XtVaSetValues(w_data->publish_combo,
					  XmNvisibleItemCount, (XtPointer)n,
					  0);
	} else {
		Widget arrow = XtNameToWidget(w_data->publish_combo, "ComboBoxArrow");
		XtVaSetValues(arrow, XmNsensitive, False, 0);
	}

	/*
	 *    Setup lower half of dialog.
	 */
	if (location_dir != NULL) {
		n = XP_STRLEN(location_dir) + 1;
		if (location_file != NULL)
			n += XP_STRLEN(location_file) + 1;

		location = (char*)XP_ALLOC(n);
		XP_STRCPY(location, location_dir);
		
		if (location_file != NULL)
			XP_STRCAT(location, location_file);
	} else {
		location = NULL;
	}

	save_password = (password != NULL);
	
	if (location) {
	    fe_TextFieldSetString(w_data->publish_location_text, location, FALSE);
		XP_FREE(location);
	}
	
	if (username) {
	    fe_TextFieldSetString(w_data->publish_username_text, username, FALSE);
		XP_FREE(username);
	}
	
	if (password) {
	    fe_TextFieldSetString(w_data->publish_password_text, password, FALSE);
		memset(password, 0, strlen(password));
		XP_FREE(password);
	}

	/*
	 *    Enable "Use Default Location"?
	 */
	fe_EditorPreferencesGetPublishLocation(context, &location, NULL, NULL);
	if (location) {
		has_default = TRUE;
		XP_FREE(location);
	} else {
		has_default = FALSE;
	}

	XtVaSetValues(w_data->publish_use_default, XmNsensitive, has_default, 0);
	XtVaSetValues(w_data->publish_save_password, XmNset, save_password, 0);
	
}

Boolean
fe_EditorPublishFiles(MWContext* context,
					  char*      target_file,
					  char**     source_files,
					  char*      username,
					  char*      password)
{
	char*          full_location = NULL; /* MUST set to NULL */
	Boolean        rv;
	Boolean        links;
	Boolean        images;
	char*          primary_url = NULL;
	History_entry* hist_entry;

	rv = NET_MakeUploadURL(&full_location, target_file,
						   username, password);

	if (rv && full_location != NULL) {

		/* Get current history entry for source location */
		hist_entry = SHIST_GetCurrent(&context->hist);
		if (hist_entry != NULL
			&& hist_entry->address != NULL
			&& hist_entry->address[0] != '\0') {
			primary_url = hist_entry->address;
		} else {
			/* no source name. */
			/* Is there a define for file:///Untitled somewhere? */
			primary_url = "file:///Untitled";
		}

		fe_EditorPreferencesGetLinksAndImages(context, &links, &images);

		/*
		 *    NOTE: we must donate source_files to the back-end,
		 *    it will be freed there. All other arguments we own.
		 */
		EDT_PublishFile(context, ED_FINISHED_REVERT_BUFFER,
						primary_url,
						source_files, full_location,
						images, links, FALSE);

		XP_FREE(full_location);

		return TRUE;
	} else {
		return FALSE;
	}
}

static Boolean
fe_publish_dialog_validate(MWContext* context, fe_PublishDialogStruct* w_data)
{
	char* target_file;
	Boolean rv  = TRUE;

	target_file = fe_TextFieldGetString(w_data->publish_location_text);

	fe_StringTrim(target_file);

	/*
	 *    Use new call in BE. This guy also posts the alert (via FE_Confirm).
	 *    So we don't need to do a dialog ourselves. But, we don't get to
	 *    place it either, sigh.
	 */
	if (!EDT_CheckPublishURL(context, target_file))
		rv = FALSE;

	XtFree(target_file);
	return rv;
}

static void
fe_publish_dialog_set(MWContext* context, fe_PublishDialogStruct* w_data)
{
    int* items;
	int  nitems;
	int n;
	int item;
	char* target_file;
	char* username;
	char* password;
	char** source_files;
	Boolean  save_password;
	char* title;

	XmListGetSelectedPos(w_data->local_list, &items, &nitems);

	source_files = (char**)XP_ALLOC(sizeof(char*) * (nitems + 2));

	for (n = 0; n < nitems; n++) {
		item = items[n] - 1; /* XmList offset */
		source_files[n] = XP_STRDUP(w_data->files[item]); /* must do this */
	}
	source_files[n] = NULL;

	if (nitems > 0 && items != NULL) /* sometimes get non-zero items */
		XtFree((void*)items);

	target_file = fe_TextFieldGetString(w_data->publish_location_text);
	username = fe_TextFieldGetString(w_data->publish_username_text);
	password = fe_GetPasswdText(w_data->publish_password_text);

	fe_StringTrim(target_file);
	fe_StringTrim(username);
	fe_StringTrim(password);

	/*
	 *    Save the location for next time.
	 *
	 *    Ok, this is totally wierd (but totally Motif). We were
	 *    doing a GetValues() here, and we were doing it after the
	 *    call to PublishFiles(). We would die in Xt event dispatch,
	 *    with looked like a bogus mapnotify event. I suspect that
	 *    this had something to do with the GetValues. PushBG's
	 *    GetValues() does seem someone complicated, so 1) let's use
	 *    a GetState() call (which just reads the boolean value in
	 *    the widget), and do this before the call to publish,
	 *    which we should probably do anyway.
	 */
	save_password =
		XmToggleButtonGadgetGetState(w_data->publish_save_password);

	fe_EditorDefaultSetLastPublishLocation(context,
										   target_file,
										   username,
										   save_password? password: NULL);

	/* title */
	title = fe_TextFieldGetString(w_data->title_text);

	if (title != NULL) {
		fe_EditorDocumentSetTitle(context, title);
		XtFree(title);
	}

	fe_EditorPublishFiles(context, target_file,
						  source_files, username, password);

	XtFree(target_file);
	XtFree(username);
	memset(password, 0, strlen(password));
	XtFree(password);
}

static void
fe_publish_dialog_delete(MWContext* context, fe_PublishDialogStruct* w_data)
{
    if (w_data->url_pathname)
	    XtFree(w_data->url_pathname);
	free_string_array(w_data->files);
}

static void
fe_publish_dialog_create(MWContext* context, Widget parent,
						 fe_PublishDialogStruct* w_data)
{
    Widget form;
	Widget local_frame;
	Widget local_form;
	Widget local_publish_label;
	Widget local_publish_info;
	Widget local_include_label;
	Widget local_include_radio;
	Widget local_include_files;
	Widget local_include_images;
	Widget local_select_none;
	Widget local_select_all;
	Widget local_list;
	Widget list_parent;

	Widget publish_frame;
	Widget publish_form;
	Widget publish_label;
	Widget publish_drop;
	Widget publish_username_label;
	Widget publish_username_text;
	Widget publish_use_default;
	Widget publish_password_label;
	Widget publish_password_text;
	Widget publish_password_save;

	Widget title_frame;
	Widget title_text;

	Widget children[16];
	Cardinal nchildren;
	Cardinal n;
	Arg args[16];
	Dimension left_offset;
	Pixel parent_bg;
	Pixel select_bg;
	Dimension width;
	Widget fat_guy;

	Visual *v = 0;
	Colormap cmap = 0;
	Cardinal depth = 0;

	n = 0;
	form = XmCreateForm(parent, "publish", args, n);
	XtManageChild(form);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	local_frame = fe_CreateFrame(form, "localFiles", args, n);
	XtManageChild(local_frame);

	n = 0;
	local_form = XmCreateForm(local_frame, "localFiles", args, n);
	XtManageChild(local_form);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	local_publish_label = XmCreateLabelGadget(local_form, "publishLabel",
											  args, n);
	children[nchildren++] = local_publish_label;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	local_include_label = XmCreateLabelGadget(local_form, "includeLabel",
											  args, n);
	children[nchildren++] = local_include_label;

	n = 0;
	local_select_none = XmCreatePushButtonGadget(local_form, "selectNone",
											  args, n);
	children[nchildren++] = local_select_none;

	n = 0;
	local_select_all = XmCreatePushButtonGadget(local_form, "selectAll",
											  args, n);
	children[nchildren++] = local_select_all;

	left_offset = fe_get_offset_from_widest(children, nchildren);

	fat_guy = XfeBiggestWidget(TRUE, &children[nchildren-2], 2);
	XtVaGetValues(fat_guy, XmNwidth, &width, 0);

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	local_publish_info = XmCreateLabelGadget(local_form, "publishInfo",
											 args, n);
	children[nchildren++] = local_publish_info;

	n = 0;
	XtSetArg(args[n], XmNradioBehavior, TRUE); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	local_include_radio = XmCreateRowColumn(local_form, "includeRadio",
											args, n);
	children[nchildren++] = local_include_radio;

	n = 0;
	XtSetArg(args[n], XmNbackground, &parent_bg); n++;
	XtGetValues(local_form, args, n);
	
	XmGetColors(XtScreen(parent), fe_cmap(context),
				parent_bg, NULL, NULL, NULL, &select_bg);

	n = 0;
	XtSetArg(args[n], XmNvisibleItemCount, 5); n++;
	XtSetArg(args[n], XmNselectionPolicy, XmMULTIPLE_SELECT); n++;
	XtSetArg(args[n], XmNbackground, select_bg); n++;
	local_list = XmCreateScrolledList(local_form, "includeList",
											  args, n);
	children[nchildren++] = list_parent = XtParent(local_list);

	/*
	 *    Now do attachments.
	 */
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetValues(local_publish_label, args, n);

	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, local_publish_info); n++;
	XtSetValues(local_include_label, args, n);

#if 0
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, local_include_label); n++;
	XtSetArg(args[n], XmNwidth, width); n++;
	XtSetValues(local_select_none, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, local_select_none); n++;
	XtSetArg(args[n], XmNwidth, width); n++;
	XtSetValues(local_select_all, args, n);

#else
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, local_select_all); n++;
	XtSetArg(args[n], XmNwidth, width); n++;
	XtSetValues(local_select_none, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
#if 0
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
#else
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, list_parent); n++;
#endif
	XtSetArg(args[n], XmNwidth, width); n++;
	XtSetValues(local_select_all, args, n);
#endif

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetValues(local_publish_info, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, local_publish_info); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, local_publish_info); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetValues(local_include_radio, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, local_include_radio); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, local_publish_info); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
#if 0
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
#endif
	XtSetValues(list_parent, args, n);
	
	XtManageChildren(children, nchildren);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	local_include_images = XmCreateToggleButtonGadget(local_include_radio,
													  "includeImages",
													  args, n);
	children[nchildren++] = local_include_images;

	n = 0;
	XtSetArg(args[n], XmNindicatorType, XmONE_OF_MANY); n++;
	local_include_files = XmCreateToggleButtonGadget(local_include_radio,
													  "includeAll",
													  args, n);
	children[nchildren++] = local_include_files;

	XtManageChildren(children, nchildren);

	XtManageChild(local_list);

	XtAddCallback(local_select_none, XmNactivateCallback,
				  fe_publish_include_select_none_cb, (XtPointer)w_data);

	XtAddCallback(local_select_all, XmNactivateCallback,
				  fe_publish_include_select_all_cb, (XtPointer)w_data);

	XtAddCallback(local_include_images, XmNvalueChangedCallback,
				  fe_publish_include_image_files_cb, (XtPointer)w_data);

	XtAddCallback(local_include_files, XmNvalueChangedCallback,
				  fe_publish_include_all_files_cb, (XtPointer)w_data);

	/*
	 *    Title group.
	 */
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, local_frame); n++;
	title_frame = fe_CreateFrame(form, "titleFrame", args, n);
	XtManageChild(title_frame);

	n = 0;
	title_text = fe_CreateTextField(title_frame, "titleText", args, n);
	XtManageChild(title_text);
	
	/*
	 *    Publish group.
	 */
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, title_frame); n++;
	publish_frame = fe_CreateFrame(form, "publishLocation", args, n);
	XtManageChild(publish_frame);

	n = 0;
	publish_form = XmCreateForm(publish_frame, "publishLocation", args, n);
	XtManageChild(publish_form);

	nchildren = 0;
	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	publish_label = XmCreateLabelGadget(publish_form, "publishLabel", args, n);
	children[nchildren++] = publish_label;

	n = 0;
#if 0
	publish_drop = fe_CreateTextField(publish_form, "publishDrop", args, n);
#else
	XtVaGetValues(CONTEXT_WIDGET(context), XtNvisual, &v, XtNcolormap, &cmap,
				  XtNdepth, &depth, 0);

	n = 0;
	XtSetArg(args[n], XmNvisual, v); n++;
	XtSetArg(args[n], XmNdepth, depth); n++;
	XtSetArg(args[n], XmNcolormap, cmap); n++;
	XtSetArg(args[n], XmNtype, XmDROP_DOWN_COMBO_BOX); n++;
	XtSetArg(args[n], XmNshadowThickness, 1); n++;
	XtSetArg(args[n], XmNmarginWidth, 0); n++;
	XtSetArg(args[n], XmNmarginHeight, 0); n++;
	XtSetArg(args[n], XmNarrowType, XmMOTIF); n++;
	XtSetArg(args[n], XmNupdateLabel, False); n++;
	publish_drop = DtCreateComboBox(publish_form,
									"publishDrop",
									args, n);
	XtAddCallback(publish_drop, XmNselectionCallback,
				  fe_publish_combo_selection_cb, w_data);
#endif
	children[nchildren++] = publish_drop;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	publish_username_label = XmCreateLabelGadget(publish_form, "usernameLabel",
												 args, n);
	children[nchildren++] = publish_username_label;

	n = 0;
	XtSetArg(args[n], XmNalignment, XmALIGNMENT_BEGINNING); n++;
	publish_password_label = XmCreateLabelGadget(publish_form, "passwordLabel",
												 args, n);
	children[nchildren++] = publish_password_label;

	left_offset = fe_get_offset_from_widest(&children[nchildren-2], 2);

	n = 0;
	publish_username_text = fe_CreateTextField(publish_form, "usernameText",
											   args, n);
	children[nchildren++] = publish_username_text;

	n = 0;
	publish_use_default = XmCreatePushButtonGadget(publish_form,
												   "useDefault",
												   args, n);
	children[nchildren++] = publish_use_default;

	XtAddCallback(publish_use_default, XmNactivateCallback,
				  fe_publish_use_default_cb, (XtPointer)w_data);

	n = 0;
	XtSetArg(args[n], XmNmaxLength, 1024); n++;
	publish_password_text = fe_CreatePasswordField(publish_form,
												   "passwordText",
												   args, n);
	children[nchildren++] = publish_password_text;

	n = 0;
	publish_password_save = XmCreateToggleButtonGadget(publish_form,
													   "savePassword",
													   args, n);
	children[nchildren++] = publish_password_save;
	
	/*
	 *    Attachments.
	 */
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetValues(publish_label, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_label); n++;
	XtSetValues(publish_drop, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_drop); n++;
	XtSetValues(publish_username_label, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_username_text); n++;
	XtSetValues(publish_password_label, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_drop); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftOffset, left_offset); n++;
	XtSetValues(publish_username_text, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_username_text); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, publish_username_text); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, publish_username_text); n++;
	XtSetValues(publish_password_text, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, publish_username_text); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_drop); n++;
	XtSetValues(publish_use_default, args, n);
	
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, publish_use_default); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, publish_username_text); n++;
	XtSetValues(publish_password_save, args, n);
	
	XtManageChildren(children, nchildren);

	w_data->local_publish_info = local_publish_info;
    w_data->local_include_images = local_include_images;
    w_data->local_include_files = local_include_files;
    w_data->local_list = local_list;
    w_data->local_select_all = local_select_all;
    w_data->local_select_none = local_select_none;
    w_data->publish_combo = publish_drop;
	XtVaGetValues(publish_drop,
				  XmNtextField, &w_data->publish_location_text,
				  0);

	w_data->title_text = title_text;

    w_data->publish_username_text = publish_username_text;
    w_data->publish_password_text = publish_password_text;
    w_data->publish_save_password = publish_password_save;
	w_data->publish_use_default = publish_use_default;
	w_data->context = context;
	w_data->url_pathname = NULL;
	w_data->files = NULL;
}

Boolean
fe_EditorPublishDialogDo(MWContext* context)
{
    fe_PublishDialogStruct publish;
	Widget  dialog;
	int     done;
	Boolean rv;

	/*
	 *    Make prompt with ok, cancel, no apply, separator.
	 */
	dialog = fe_CreatePromptDialog(context, "publishFilesDialog",
								   TRUE, TRUE, FALSE, FALSE, TRUE);

	fe_publish_dialog_create(context, dialog, &publish);
	fe_publish_dialog_init(context, &publish);

	/*
	 *   Add a bunch of callbacks to the buttons.
	 */
	XtAddCallback(dialog, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(dialog, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(dialog, XmNdestroyCallback, fe_hrule_destroy_cb, &done);

    /*
     *    Popup.
     */
    XtManageChild(dialog);

	/*
     *    Wait.
     */
	fe_NukeBackingStore(dialog); /* what does this do? */

	done = XmDIALOG_NONE;
	while (done == XmDIALOG_NONE) {
		fe_EventLoop();

		if (done == XmDIALOG_OK_BUTTON) {
			if (!fe_publish_dialog_validate(context, &publish)) {
				done = XmDIALOG_NONE;
			}
		}
	}

	/* something is wrong here - benjie */
	/* if you put in a wrong domain, but with http:// and ftp:// 
	 * the following call will fail, and then we loose btn callback 
	 * action and hangs, the problem traces to the exit routine
	 * we send in to NET_GetURL. For the editor, the exit routine,
	 * after poping up the error message, calls fe_EditorGetURLExitRoutine,
	 * which calls fe_EventLoop, and the code loops in there, but don't
  	 * pick up click actions on the buttons in this dialog
     */

	/* ok, so the following line temporarilly fix the above problem 
	 * by poping down the modal dialog.
	 * this fixes 26903. - benjie
     */
	XtUnmanageChild(dialog); 

	if (done == XmDIALOG_OK_BUTTON) {
		fe_publish_dialog_set(context, &publish);
		rv = TRUE;
 	} else {
	    rv = FALSE;
	}

	fe_publish_dialog_delete(context, &publish);

	if (done != XFE_DIALOG_DESTROY_BUTTON) 
		XtDestroyWidget(dialog); 

    return rv;
}

int
fe_YesNoCancelDialog(MWContext* context, char* name, char* message)
{
	Widget dialog;
	Widget mainw = CONTEXT_WIDGET(context);
	Widget yes_button;
	Widget no_button;
	Arg    args[8];
	Cardinal n;
	Visual*  v;
	Colormap cmap;
	Cardinal depth;
	int done;
	XmString xm_message;

	/*
	 *    Popup the shell first, so that we gurantee its realized 
	 */
	XtPopup(mainw, XtGrabNone);
	
	/*
	 *    Force the window to the front and de-iconify if needed 
	 */
	XMapRaised(XtDisplay(mainw), XtWindow(mainw));
	
	XtVaGetValues(mainw, XtNvisual, &v, XtNcolormap, &cmap,
				  XtNdepth, &depth, 0);

	xm_message = XmStringCreateLtoR(message,XmFONTLIST_DEFAULT_TAG);
	n = 0;
	XtSetArg(args[n], XmNvisual, v); n++;
	XtSetArg(args[n], XmNdepth, depth); n++;
	XtSetArg(args[n], XmNcolormap, cmap); n++;
	XtSetArg(args[n], XmNtransientFor, mainw); n++;
  	XtSetArg(args[n], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); n++;
	XtSetArg(args[n], XmNdefaultButtonType, XmDIALOG_OK_BUTTON); n++;
	XtSetArg(args[n], XmNmessageString, xm_message); n++;
	dialog = XmCreateMessageDialog(mainw, name, args, n);
	XmStringFree(xm_message);

	/* Doesn't work as a create argument :-) */
	n = 0;
	XtSetArg(args[n], XmNdialogType, XmDIALOG_QUESTION); n++;
	XtSetValues(dialog, args, n);

#ifdef NO_HELP
	fe_UnmanageChild_safe(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
#endif

	n = 0;
	yes_button = XmCreatePushButtonGadget(dialog, "yes", args, n);
	XtManageChild(yes_button);

	n = 0;
	no_button = XmCreatePushButtonGadget(dialog, "no", args, n);
	XtManageChild(no_button);

	fe_UnmanageChild_safe(XmMessageBoxGetChild(dialog, XmDIALOG_OK_BUTTON));

	XtAddCallback(dialog, XmNcancelCallback, fe_hrule_cancel_cb, &done);
	XtAddCallback(dialog, XmNdestroyCallback, fe_hrule_destroy_cb, &done);
	XtAddCallback(yes_button, XmNactivateCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(no_button, XmNactivateCallback, fe_hrule_apply_cb, &done);

	XtManageChild(dialog);

	done = XmDIALOG_NONE;
	while (done == XmDIALOG_NONE) {
		fe_EventLoop();
	}
	return done;
}

Boolean
fe_HintDialog(MWContext* context, char* message)
{
	Widget dialog;
	Widget mainw = CONTEXT_WIDGET(context);
	Arg    args[8];
	Cardinal n;
	Visual*  v;
	Colormap cmap;
	Cardinal depth;
	int done;
	Widget toggle_button;
	Widget toggle_row;
	XmString xm_message;
	Boolean  return_value;

	XtVaGetValues(mainw, XtNvisual, &v, XtNcolormap, &cmap,
				  XtNdepth, &depth, 0);

	xm_message = XmStringCreateLocalized(message);

	n = 0;
	XtSetArg(args[n], XmNvisual, v); n++;
	XtSetArg(args[n], XmNdepth, depth); n++;
	XtSetArg(args[n], XmNcolormap, cmap); n++;
	XtSetArg(args[n], XmNtransientFor, mainw); n++;
	XtSetArg(args[n], XmNdefaultButtonType, XmDIALOG_OK_BUTTON); n++;
	XtSetArg(args[n], XmNmessageString, xm_message); n++;
	dialog = XmCreateInformationDialog(mainw, "hintDialog", args, n);
	XmStringFree(xm_message);

	/*
	 *    This is sooooooo lame. Dispite what the manual says, an
	 *    additonal toggle button child is not placed above the push
	 *    buttons, but rather along aside the. Stick the toggle in
	 *    a row, just to get the thing to do what we want.
	 */
	n = 0;
	toggle_row = XmCreateRowColumn(dialog, "dontDisplayAgainRow", args, n);
	XtManageChild(toggle_row);
	n = 0;
	XtSetArg(args [n], XmNindicatorType, XmN_OF_MANY); n++;
	toggle_button = XmCreateToggleButtonGadget(toggle_row, "dontDisplayAgain",
											   args, n);
	XtManageChild(toggle_button);

	fe_UnmanageChild_safe(XmMessageBoxGetChild(dialog,XmDIALOG_CANCEL_BUTTON));
#ifdef NO_HELP
	fe_UnmanageChild_safe(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
#endif

	XtAddCallback(dialog, XmNokCallback, fe_hrule_ok_cb, &done);
	XtAddCallback(dialog, XmNdestroyCallback, fe_hrule_destroy_cb, &done);
	
	XtManageChild(dialog);

	done = XmDIALOG_NONE;
        while (done == XmDIALOG_NONE) {
		fe_EventLoop();
	}

	if (done != XFE_DIALOG_DESTROY_BUTTON) {
		return_value = XmToggleButtonGetState(toggle_button);
		XtDestroyWidget(dialog);
		return return_value;
	}

	return FALSE;
}

static void
flip_toggle_cb (Widget w, XtPointer closure, XtPointer call_data)
{
    Widget toggle = (Widget)closure;

    if (XmToggleButtonGadgetGetState(w))
        XmToggleButtonGadgetSetState(toggle, FALSE, FALSE);
}

ED_CharsetEncode FE_EncodingDialog(MWContext* context, char* newCharset)
{
    ED_CharsetEncode retval = ED_ENCODE_CANCEL;
    int done;
    Widget mainw = CONTEXT_WIDGET (context);
    Widget dialog, radio, toggle1, toggle2;
    Arg av [20];
    int ac;
    Visual *v = 0;
    Colormap cmap = 0;
    Cardinal depth = 0;
    XmString xm_message;
    char* pMessage;

    XtVaGetValues (mainw, XtNvisual, &v, XtNcolormap, &cmap,
                   XtNdepth, &depth, 0);
    ac = 0;
    XtSetArg(av[ac], XmNvisual, v); ac++;
    XtSetArg(av[ac], XmNdepth, depth); ac++;
    XtSetArg(av[ac], XmNcolormap, cmap); ac++;
    XtSetArg(av[ac], XmNtransientFor, mainw); ac++;
    XtSetArg(av[ac], XmNdefaultButtonType, XmDIALOG_OK_BUTTON); ac++;
    dialog = XmCreateQuestionDialog(mainw, "changeEncoding", av, ac);

    ac = 0;
    XtSetArg(av[ac], XmNorientation, XmVERTICAL); ac++;
    radio = XmCreateRowColumn(dialog, "_radio", av, ac);
    XtManageChild(radio);

    /* newCharset needs to be plugged in to the strings in the togglebuttons */

    ac = 0;
    XtSetArg(av[ac], XmNindicatorType, XmN_OF_MANY); ac++;
    toggle1 = XmCreateToggleButtonGadget(radio, "convertPageToggle", av, ac);
    XtManageChild(toggle1);
    XmToggleButtonGadgetSetState(toggle1, TRUE, FALSE);

    pMessage = XP_GetString(XP_EDT_CHARSET_CONVERT_PAGE);
    xm_message = XmStringCreateLocalized(pMessage);
    XtVaCreateManagedWidget("convertPageLabl", xmLabelGadgetClass, radio,
                            XmNlabelString, xm_message,
                            0);
    XmStringFree(xm_message);

    ac = 0;
    XtSetArg(av[ac], XmNindicatorType, XmN_OF_MANY); ac++;
    toggle2 = XmCreateToggleButtonGadget(radio, "changeMetatagToggle", av, ac);
    XtManageChild(toggle2);

    pMessage = XP_GetString(XP_EDT_CHARSET_SET_METATAG);
    xm_message = XmStringCreateLocalized(pMessage);
    XtVaCreateManagedWidget("setMetatagLabl", xmLabelGadgetClass, radio,
                            XmNlabelString, xm_message,
                            0);
    XmStringFree(xm_message);

    /* Make the toggles mirror each other (radio behavior) */
    XtAddCallback(toggle1, XmNvalueChangedCallback, flip_toggle_cb, toggle2);
    XtAddCallback(toggle2, XmNvalueChangedCallback, flip_toggle_cb, toggle1);

#ifdef NO_HELP
    fe_UnmanageChild_safe (XmMessageBoxGetChild (dialog, XmDIALOG_HELP_BUTTON));
#endif

    XtAddCallback(dialog, XmNokCallback, fe_hrule_ok_cb, &done);
    XtAddCallback(dialog, XmNcancelCallback, fe_hrule_cancel_cb, &done);

    XtManageChild (dialog);

    done = XmDIALOG_NONE;
    while (done == XmDIALOG_NONE) {
        fe_EventLoop();
    }

    if (done != XmDIALOG_OK_BUTTON)
    {
        retval = ED_ENCODE_CANCEL;
    }
    else if (XmToggleButtonGetState(toggle1))
    {
        retval = ED_ENCODE_CHANGE_METATAG;
    }
    else
    {
        retval = ED_ENCODE_CHANGE_CHARSET;
    }
    XtDestroyWidget(dialog);
    return retval;
}

#endif
