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

/* 
 *  colorpicker.c --- Color related widgets, dialogs, and stuff.
 *
 *  Created: David Williams <djw@netscape.com>, Apr-20-1997
 *
 *  RCSID: "$Id: colorpicker.c,v 3.1 1998/03/28 03:19:56 ltabb Exp $"
 */

#include "mozilla.h"
#include "xfe.h"
#include <XmL/Folder.h>   /* tab folder stuff */
#include <Xm/Label.h> 
#include <Xm/DrawnBP.h> 
#include <Xm/Scale.h>
#include <Xfe/XfeP.h>			/* for xfe widgets and utilities */
#include <xpgetstr.h>     /* for XP_GetString() */
#include "felocale.h"

#define DEFAULT_NCOLUMNS     10
#define DEFAULT_NROWS        7
#define DEFAULT_NCUSTOM_ROWS 2
#define DEFAULT_BUTTON_SIZE  18

#define DEFAULT_SPACE_SIZE   2
#define DEFAULT_MARGIN_SIZE  2
/*
 * NOTE: a good size seems to be something between 1 or 4...
 *
 */

#define XFE_COLOR_RED   (0x1)
#define XFE_COLOR_GREEN (0x2)
#define XFE_COLOR_BLUE  (0x4)
#define XFE_COLOR_UNITS (0x8)
#define XFE_COLOR_INIT  (0x10)

/*
 * NOTE:  new palette per EditorSpec...
 *
 */
static char* fe_color_picker_standard_palette[] = {

	"#FFFFFF","#FFCCCC","#FFCC99","#FFFF99","#FFFFCC","#99FF99","#99FFFF","#CCFFFF","#CCCCFF","#FFCCFF",
	"#CCCCCC","#FF6666","#FFCC33","#FFFF66","#FFFF99","#66FF99","#33FFFF","#66FFFF","#9999FF","#FF99FF",
	"#CCCCCC","#FF0000","#FF9900","#FFCC66","#FFFF00","#33FF33","#66CCCC","#33CCFF","#6666CC","#CC66CC",
	"#999999","#CC0000","#FF6600","#FFCC33","#FFCC00","#33CC00","#00CCCC","#3366FF","#6633FF","#CC33CC",
	"#666666","#990000","#CC6600","#CC9933","#999900","#009900","#339999","#3333FF","#6600CC","#993399",
	"#333333","#660000","#993300","#996633","#666600","#006600","#336666","#000099","#333399","#663366",
	"#000000","#330000","#663300","#663333","#333300","#003300","#003333","#000066","#330099","#330033",

	NULL
};

#if 0
/*
 *    These colors are the above corrected to fall into the 140 HTML
 *    colornames defined in lib/xp/xp_rgb.c
 */
static char* corrected_named_shuang_palette[] = {
    "white",

    "pink",
    "salmon",
    "red",
    "red",
    "darkred",
    "maroon",
    "black",

    "lightgrey",

    "lemonchiffon",
    "khaki",
    "yellow",
    "gold",
    "olive",
    "olive",
    "darkgreen",

    "darkgray",

    "lightcyan",
    "aquamarine",
    "deepskyblue",
    "royalblue",
    "royalblue",
    "darkblue",
    "navy",

    "dimgray",

    "lavender",
    "violet",
    "orchid",
    "mediumorchid",
    "darkorchid",
    "darkslateblue",
    "midnightblue",

    "black",

    "navajowhite",
    "gold",
    "orange",
    "orangered",
    "chocolate",
    "saddlebrown",
    "saddlebrown",

    "black",

    "aquamarine",
    "lightgreen",
    "limegreen",
    "limegreen",
    "green",
    "darkgreen",
    "darkgreen",
	NULL
};

/*
 *    These are the 16 color names specified in the HTML 3.2 spec.
 */
static char* html32_named_colors[] = {

	"#000000", /*Black = */ 
	"#008000", /*Green = */ 
    "#C0C0C0", /*Silver = */
	"#00FF00", /*Lime = */
	"#808080", /*Gray = */
	"#808000", /*Olive = */
	"#FFFFFF", /*White = */
	"#FFFF00", /*Yellow = */
	"#800000", /*Maroon = */
	"#000080", /*Navy = */
	"#FF0000", /*Red = */
	"#0000FF", /*Blue = */
	"#800080", /*Purple = */
	"#008080", /*Teal = */
	"#FF00FF", /*Fuchsia = */
	"#00FFFF", /*Aqua = */
	NULL
};
#endif /* end of unused palettes */

/*
 *    This routine is used throughout this code to create a pixel
 *    from an RGB value.
 */
static Pixel
fe_color_get_pixel(Widget widget, uint8 red, uint8 green, uint8 blue)
{
	MWContext* context = (MWContext *)fe_WidgetToMWContext(widget);

	/*
	 *    Make a pixel. I'm hoping Jamie's color stuff will
	 *    know when to release this...djw
	 */
	return fe_GetPixel(context, red, green, blue);
}

/*
 *    Code for a simple color swatch:
 *
 *    Widget fe_CreateSwatch(Widget parent, char* name, Arg* args, Cardinal n);
 *    void   fe_SwatchSetColor(Widget widget, LO_Color* color);
 */
static void
fe_swatch_expose_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	XmDrawnButtonWidget db = (XmDrawnButtonWidget)widget;
	XmLabelPart*        lp = &(db->label);
	GC gc;
	unsigned offset = db->primitive.highlight_thickness +
		              db->primitive.shadow_thickness;

	XtVaGetValues(widget, XmNuserData, &gc, 0);
	
	XFillRectangle(XtDisplay(widget), XtWindow(widget),
				   gc,
				   offset, offset,
				   _XfeWidth(db) - 2 * offset, _XfeHeight(db) - 2 * offset);

	if (lp->label_type == XmSTRING && lp->_label != NULL) {
		_XmStringDraw(XtDisplay(widget), XtWindow(widget),
					  lp->font, lp->_label,
					  lp->normal_GC,
					  lp->TextRect.x, lp->TextRect.y,
					  lp->TextRect.width, lp->alignment,
					  lp->string_direction, NULL);
	}
}

static void
fe_SwatchSetColor(Widget widget, LO_Color* color)
{
	GC        gc;
	XGCValues values;
	XtGCMask  valueMask;
	Pixel     pixel;
	Pixel     foreground;

	XtVaGetValues(widget, XmNuserData, &gc, 0);

	XtReleaseGC(widget, gc);

	pixel = fe_color_get_pixel(widget, color->red, color->green, color->blue);

	XmGetColors(XtScreen(widget), XfeColormap(widget), pixel,
				&foreground, NULL, NULL, NULL);

	valueMask = GCForeground;
	values.foreground = pixel;
	gc = XtGetGC(widget, valueMask, &values);

	XtVaSetValues(widget, XmNuserData, gc, XmNforeground, foreground, 0);

	if (XtIsRealized(widget))
		fe_swatch_expose_cb(widget, NULL, NULL);
}

static void
fe_swatch_destroy_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	GC gc;

	XtVaGetValues(widget, XmNuserData, &gc, 0);

	if (gc != 0)
		XtReleaseGC(widget, gc);
}

static Widget
fe_CreateSwatch(Widget parent, char* name, Arg* args, Cardinal n)
{
	Widget    widget;
	GC        gc;
	XGCValues values;
	XtGCMask  valueMask;
	Pixel     pixel;

	widget = XmCreateDrawnButton(parent, name, args, n);

	XtVaGetValues(parent, XmNbackground, &pixel, 0);

	valueMask = GCForeground;
	values.foreground = pixel;
	gc = XtGetGC(widget, valueMask, &values);

	XtVaSetValues(widget, XmNuserData, gc, 0);

	XtAddCallback(widget, XmNexposeCallback, fe_swatch_expose_cb, NULL);
	XtAddCallback(widget, XmNdestroyCallback, fe_swatch_destroy_cb, NULL);

	return widget;
}

/*
 *    Code for a color swatch matrix:
 *
 *    Widget fe_CreateSwatchMatrix(Widget parent, char* name, Arg*, Cardinal);
 *    use this function in a callback to get the color at x,y
 *    int    fe_SwatchMatrixGetColor(Widget, Position x, Position y, XColor*);
 */
#if 0
static char fe_ColorPickerButtonSize[] =         "colorButtonSize";
#endif
static char fe_ColorPickerPalette[] =            "colorPalette";

typedef struct ColorInfo {
	unsigned long rgb;
	Pixel         pixel;
} ColorInfo;

typedef struct SwatchesInfo {
	ColorInfo* colors;
	unsigned   ncolors;
	GC         drawing_gc;
} SwatchesInfo;

static void
fe_swatches_destroy_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	SwatchesInfo* info;

	XtVaGetValues(widget, XmNuserData, &info, 0);

	if (info != NULL) {
		if (info->drawing_gc != 0)
			XFreeGC(XtDisplay(widget), info->drawing_gc);

		if (info->colors != NULL)
			XtFree((char*)info->colors);

		XtFree((char*)info);
	}
}

int
fe_SwatchMatrixGetColor(Widget widget, Position p_x, Position p_y,
						XColor* color_r)
{
	char*    name = XtName(widget);
	uint8    red;
	uint8    green;
	uint8    blue;
		
	if (name == NULL) {
		red = blue = green = 0;
	}
	else {
		if (!LO_ParseRGB(name, &red, &green, &blue)) {
			red = blue = green = 0; /* black */
		}
	}

	/*
	 * NOTE:  we could also just get the background pixel from the widget
	 *        - something to look into if this becomes too expensive...
	 *
	 */
	color_r->pixel = fe_color_get_pixel(widget, red, green, blue);

	color_r->red = red;
	color_r->green = green;
	color_r->blue = blue;

	return 1;
}

typedef enum {
	XFE_COLOR_DECIMAL,
	XFE_COLOR_HEX,
	XFE_COLOR_PERCENT
} fe_ColorUnit;

typedef struct fe_SamplePageInfo fe_SamplePageInfo;

typedef struct fe_ColorUpdateInfo
{
	LO_Color     color;
	Pixel        pixel;
	fe_ColorUnit units;
	fe_SamplePageInfo* list;
} fe_ColorUpdateInfo;

typedef struct fe_SamplePart
{
	Widget old;
	Widget new;
} fe_SamplePart;

typedef void (*fe_ColorUpdateMethod_t)(void* page_info, Widget, unsigned);

struct fe_SamplePageInfo
{
	fe_ColorUpdateInfo*        update_info;
	fe_ColorUpdateMethod_t update_method;
	fe_SamplePageInfo*         next;
	fe_SamplePart              sample;
};

typedef struct fe_RgbPart
{
	Widget max;
	Widget red_scale;
	Widget green_scale;
	Widget blue_scale;
	Widget red_text;
	Widget green_text;
	Widget blue_text;
	Widget units;
} fe_RgbPart;

typedef struct fe_RgbPageInfo
{
	fe_ColorUpdateInfo* update_info;
	fe_ColorUpdateMethod_t update_method;
	fe_SamplePageInfo*         next;
	fe_SamplePart       sample;
	fe_RgbPart          rgb;
} fe_RgbPageInfo;

typedef struct fe_SwatchesPart
{
	Widget matrix;
} fe_SwatchesPart;

typedef struct fe_SwatchesPageInfo
{
	fe_ColorUpdateInfo* update_info;
	fe_ColorUpdateMethod_t update_method;
	fe_SamplePageInfo*         next;
	fe_SamplePart       sample;
	fe_SwatchesPart     swatches;
} fe_SwatchesPageInfo;

static void
fe_swatch_matrix_enter_eh(Widget chit, XtPointer closure, 
						  XEvent* event, Boolean* keep_going)
{
	XtVaSetValues(chit,
				  XmNshadowType, XmSHADOW_IN,
				  0);
}

static void
fe_swatch_matrix_exit_eh(Widget chit, XtPointer closure, 
						 XEvent* event, Boolean* keep_going)
{
	XtVaSetValues(chit,
				  XmNshadowType, XmSHADOW_ETCHED_IN,
				  0);
}

static void
fe_swatch_matrix_destroy_cb(Widget chit, XtPointer closure, XtPointer cbd)
{
	XtRemoveEventHandler(chit, EnterWindowMask, FALSE, 
						 fe_swatch_matrix_enter_eh, NULL);

	XtRemoveEventHandler(chit, LeaveWindowMask, FALSE, 
						 fe_swatch_matrix_exit_eh, NULL);
}

#define MAX_ARGS 16

Widget
fe_CreateSwatchMatrix(Widget parent, char* name, Arg* p_args, Cardinal p_nargs)
{
	Widget    matrix;
	unsigned  n;
	unsigned  m;
	Dimension shadow;
	Dimension highlight;
	Arg args[MAX_ARGS];
	SwatchesInfo* info;
	Pixel  bg_pixel = BlackPixelOfScreen(XtScreen(parent));
	Pixel  nu_pixel = NULL;
	char** palette = (char**)&fe_color_picker_standard_palette;

	n = 0;
	XtSetArg(args[n], XmNpacking, XmPACK_COLUMN); n++;
	XtSetArg(args[n], XmNnumColumns, DEFAULT_NROWS); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNspacing, DEFAULT_SPACE_SIZE); n++;
	XtSetArg(args[n], XmNmarginWidth, DEFAULT_MARGIN_SIZE); n++;
	XtSetArg(args[n], XmNmarginHeight, DEFAULT_MARGIN_SIZE); n++;
	XtSetArg(args[n], XmNtraversalOn, False); n++;
	matrix = XmCreateRowColumn(parent, name, args, n);

	/*
	 *    Parse args, looking for pallete or background spec
	 */
	for (m = 0; m < p_nargs ; m++) {
		if (strcmp(p_args[m].name, fe_ColorPickerPalette) == 0)
			palette = (char**)p_args[m].value;
		else if (strcmp(p_args[m].name, XmNbackground) == 0)
			bg_pixel = (Pixel)p_args[m].value;
	}

	/*
	 *    Make the info stuff.
	 */
	info = XtNew(SwatchesInfo);
	info->drawing_gc = 0;

	for (n = 0; palette[n] != NULL; n++)
		;

	info->ncolors = n;
	info->colors = NULL;

	for (n = 0;n < info->ncolors; n++) {
		uint8 red;
		uint8 green;
		uint8 blue;
		unsigned i = 0;
		Widget chit = NULL;
		
		if (!LO_ParseRGB(palette[n], &red, &green, &blue)) {
			red = blue = green = 0; /* black */
		}

		nu_pixel = fe_color_get_pixel(matrix, red, green, blue);

		i = 0;
		XtSetArg(args[i], XmNbackground, nu_pixel); i++;
		XtSetArg(args[i], XmNwidth, DEFAULT_BUTTON_SIZE); i++;
		XtSetArg(args[i], XmNheight, DEFAULT_BUTTON_SIZE); i++;
		XtSetArg(args[i], XmNshadowType, XmSHADOW_ETCHED_IN); i++;
		XtSetArg(args[i], XmNhighlightThickness, 0); i++;
		XtSetArg(args[i], XmNmarginWidth, DEFAULT_MARGIN_SIZE); i++;
		XtSetArg(args[i], XmNmarginHeight, DEFAULT_MARGIN_SIZE); i++;
		XtSetArg(args[i], XmNtraversalOn, False); i++;
		XtSetArg(args[i], XmNpushButtonEnabled, False); i++;

		chit = XmCreateDrawnButton(matrix, palette[n], args, i);

		fe_WidgetAddToolTips(chit);

		XtAddEventHandler(chit, EnterWindowMask, FALSE, 
						  fe_swatch_matrix_enter_eh, NULL);

		XtAddEventHandler(chit, LeaveWindowMask, FALSE, 
						  fe_swatch_matrix_exit_eh, NULL);

		XtAddCallback(chit, XmNdestroyCallback, 
					  fe_swatch_matrix_destroy_cb, NULL);

		XtManageChild(chit);
	}

	XtVaSetValues(matrix,
				  XmNuserData, info,
				  0);

	XtAddCallback(matrix, XmNdestroyCallback, fe_swatches_destroy_cb,
				  NULL);

	return matrix;
}

void
fe_AddSwatchMatrixCallback(Widget matrix, 
						   char* name, XtCallbackProc proc, XtPointer data)
{
	WidgetList kids = NULL;
	uint32  numKids = 0;
	uint32        n = 0;

	XtVaGetValues(matrix,
				  XmNchildren, &kids,
				  XmNnumChildren, &numKids,
				  0);

	if (numKids > 0) {
		for (n = 0; n < numKids; n++) {
			XtAddCallback(kids[n], name, proc, data);
		}
	}
}

static void
fe_color_install_page(fe_ColorUpdateInfo*    update,
					  fe_ColorUpdateMethod_t update_method,
					  fe_SamplePageInfo*     page)
{
	page->update_info = update;
	page->update_method = update_method;
	page->next = update->list;
	update->list = page;
}

static void 
fe_color_sample_part_update(fe_SamplePart*      widgets,
							fe_ColorUpdateInfo* update_info,
							Widget              caller,
							unsigned            mask)
{
	fe_SwatchSetColor(widgets->new, &update_info->color);

	if ((mask & XFE_COLOR_INIT) != 0) {
		Pixel foreground;
		XmGetColors(XtScreen(widgets->old), XfeColormap(widgets->old),
					update_info->pixel,
					&foreground, NULL, NULL, NULL);
		XtVaSetValues(widgets->old,
					  XmNbackground, update_info->pixel,
					  XmNforeground, foreground,
					  0);
	}
}

static void 
fe_color_rgb_page_update(fe_RgbPageInfo*, Widget, unsigned mask);

static void
fe_color_rgb_max_update_cb(Widget widget, XtPointer closure, XtPointer cb)
{
	fe_ColorUpdateInfo* info = (fe_ColorUpdateInfo*)closure;
	char* buf;
	XmString tmp_string;
	
	switch (info->units) {
	case XFE_COLOR_HEX:     buf = "0xFF"; break;
	case XFE_COLOR_PERCENT: buf = "100%"; break;
	case XFE_COLOR_DECIMAL:
	default:                buf = " 255"; break;
	}

	tmp_string = XmStringCreateSimple(buf);

	XtVaSetValues(widget, XmNlabelString, tmp_string, 0);

	XmStringFree(tmp_string);
}

static void
fe_color_rgb_text_cb(Widget widget, XtPointer closure, XtPointer cb)
{
	fe_RgbPageInfo*     rgb_info = (fe_RgbPageInfo*)closure;
	fe_ColorUpdateInfo* info = rgb_info->update_info;
	char* string = fe_GetTextField(widget);
	unsigned value;
	unsigned mask;
	char* name = XtName(widget);

	if (info->units == XFE_COLOR_HEX) {
		value = strtoul(string, NULL, 16);
	} else if (info->units == XFE_COLOR_PERCENT) {
		value = strtoul(string, NULL, 10);
		if (value > 100)
			value = 100;
		value = (256 * value) / 100;
	} else {
		value = strtoul(string, NULL, 10);
	}

	if (value > 255)
		value = 255;

	if (name[0] == 'r') { /* this sucks */
		info->color.red = value;
		mask = XFE_COLOR_RED;
	} else if (name[0] == 'g') {
		info->color.green = value;
		mask = XFE_COLOR_GREEN;
	} else {
		info->color.blue = value;
		mask = XFE_COLOR_BLUE;
	}

	fe_color_rgb_page_update(rgb_info, widget, mask);
}

static void
fe_color_rgb_text_update_cb(Widget widget, XtPointer closure, XtPointer cb)
{
	fe_ColorUpdateInfo* info = (fe_ColorUpdateInfo*)closure;
	Widget              caller = (Widget)cb;
	char                buf[32];
	unsigned big;
	unsigned result;
	unsigned value;
	char* name;

	if (caller == widget)
		return;

	name = XtName(widget);

	if (name[0] == 'r') /* this sucks */
		value = info->color.red;
	else if (name[0] == 'g')
		value = info->color.green;
	else
		value = info->color.blue;
	
	switch (info->units) {
	case XFE_COLOR_HEX:     
		sprintf(buf, "%02X", value);
		break;
	case XFE_COLOR_PERCENT: 
		big = 100 * value;
		result = big / 256;
		if ((big % 256) > 127)
			result++;
		sprintf(buf, "%03d", result);
		break;
	case XFE_COLOR_DECIMAL:
	default:
		sprintf(buf, "%03d", value);
		break;
	}

	fe_TextFieldSetString(widget, buf, False);
}

static void
fe_color_rgb_scale_cb(Widget widget, XtPointer closure, XtPointer cb)
{
	fe_RgbPageInfo*     rgb_info = (fe_RgbPageInfo*)closure;
	fe_ColorUpdateInfo* info = rgb_info->update_info;
	XmScaleCallbackStruct* cd = (XmScaleCallbackStruct*)cb;
	unsigned value = cd->value;
	char* name = XtName(widget);
	unsigned mask;
	
	if (value > 255)
		value = 255;

	if (name[0] == 'r') { /* this sucks */
		info->color.red = value;
		mask = XFE_COLOR_RED;
	} else if (name[0] == 'g') {
		info->color.green = value;
		mask = XFE_COLOR_GREEN;
	} else {
		info->color.blue = value;
		mask = XFE_COLOR_BLUE;
	}

	fe_color_rgb_page_update(rgb_info, widget, mask);
}

static void
fe_color_rgb_scale_update_cb(Widget widget, XtPointer closure, XtPointer cb)
{
	fe_ColorUpdateInfo* info = (fe_ColorUpdateInfo*)closure;
	Widget caller = (Widget)cb;
	unsigned value;
	char* name;

	if (caller == widget)
		return;

	name = XtName(widget);

	if (name[0] == 'r') /* this sucks */
		value = info->color.red;
	else if (name[0] == 'g')
		value = info->color.green;
	else
		value = info->color.blue;

	XtVaSetValues(widget, XmNvalue, value, 0);
}

static void
fe_color_rgb_units_cb(Widget widget, XtPointer closure, XtPointer cb)
{
	fe_RgbPageInfo*     rgb_info = (fe_RgbPageInfo*)closure;
	fe_ColorUpdateInfo* info = rgb_info->update_info;
	char* name = XtName(widget);

	if (name[0] == 'p') /* percent */
		info->units = XFE_COLOR_PERCENT;
	else if (name[0] == 'h')
		info->units = XFE_COLOR_HEX;
	else
		info->units = XFE_COLOR_DECIMAL;

	fe_color_rgb_page_update(rgb_info, widget, XFE_COLOR_UNITS);
}

static void
fe_color_rgb_page_update(fe_RgbPageInfo* rgb_info, Widget caller, unsigned mask)
{
	fe_ColorUpdateInfo* info = rgb_info->update_info;
	fe_SamplePart*      sample_part = &rgb_info->sample;
	fe_RgbPart*         rgb_part = &rgb_info->rgb;

	if ((mask & (XFE_COLOR_RED|XFE_COLOR_GREEN|XFE_COLOR_BLUE)) != 0) {

		info->pixel = fe_color_get_pixel(sample_part->new, 
										 info->color.red,
										 info->color.green,
										 info->color.blue);

		/*
		 *    Update the sample
		 */
		fe_color_sample_part_update(&rgb_info->sample, info, caller, mask);
	}

	if ((mask & XFE_COLOR_RED) != 0)
		fe_color_rgb_scale_update_cb(rgb_part->red_scale, info, caller);

	if ((mask & XFE_COLOR_GREEN) != 0)
		fe_color_rgb_scale_update_cb(rgb_part->green_scale, info, caller);

	if ((mask & XFE_COLOR_BLUE) != 0)
		fe_color_rgb_scale_update_cb(rgb_part->blue_scale, info, caller);

	if ((mask & XFE_COLOR_UNITS) != 0)
		fe_color_rgb_max_update_cb(rgb_part->max, info, caller);

	if ((mask & (XFE_COLOR_RED|XFE_COLOR_UNITS)) != 0)
		fe_color_rgb_text_update_cb(rgb_part->red_text, info, caller);

	if ((mask & (XFE_COLOR_GREEN|XFE_COLOR_UNITS)) != 0)
		fe_color_rgb_text_update_cb(rgb_part->green_text, info, caller);

	if ((mask & (XFE_COLOR_BLUE|XFE_COLOR_UNITS)) != 0)
		fe_color_rgb_text_update_cb(rgb_part->blue_text, info, caller);
}

static Widget
fe_create_rgb_group(Widget parent, fe_RgbPageInfo* info)
{
	Widget rgb_frame;
	Widget form;
	Widget red_label;
	Widget green_label;
	Widget blue_label;
	Widget red_scale;
	Widget green_scale;
	Widget blue_scale;
	Widget red_text;
	Widget green_text;
	Widget blue_text;
	Widget min_label;
	Widget max_label;
	Widget units;
	Widget pulldown;
	XmString tmp_string;
	Widget bottom;
	Widget text;
	Widget label;
	Widget scale;
	Widget button;
	int i;
	Widget children[16];
	Cardinal nchildren = 0;
	Arg args[16];
	Cardinal n;

#define SHOW_VALUE FALSE
#define TEXT_NCOLUMNS 4

	n = 0;
	rgb_frame = XmCreateFrame(parent, "rgbEditor", args, n);
	XtManageChild(rgb_frame);

	n = 0;
	form = XmCreateForm(rgb_frame, "rgbForm", args, n);
	XtManageChild(form);

	tmp_string = XmStringCreateSimple("0");

	n = 0;
	XtSetArg(args[n], XmNlabelType, XmSTRING); n++;
	XtSetArg(args[n], XmNlabelString, tmp_string); n++;
	min_label = XmCreateLabelGadget(form, "minLabel", args, n);
	children[nchildren++] = min_label;

	XmStringFree(tmp_string);

	tmp_string = XmStringCreateSimple("255");

	n = 0;
	XtSetArg(args[n], XmNlabelType, XmSTRING); n++;
	XtSetArg(args[n], XmNlabelString, tmp_string); n++;
	max_label = XmCreateLabelGadget(form, "maxLabel", args, n);
	children[nchildren++] = max_label;

	XmStringFree(tmp_string);

	tmp_string = XmStringCreateSimple("R");

	n = 0;
	XtSetArg(args[n], XmNlabelType, XmSTRING); n++;
	XtSetArg(args[n], XmNlabelString, tmp_string); n++;
	red_label = XmCreateLabelGadget(form, "redLabel", args, n);
	children[nchildren++] = red_label;

	XmStringFree(tmp_string);
	
	n = 0;
	XtSetArg(args[n], XmNuserData, (XtPointer)XFE_COLOR_RED); n++;
	XtSetArg(args[n], XmNshowValue, SHOW_VALUE); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNmaximum, 256); n++;
	red_scale = XmCreateScale(form, "redScale", args, n);
	children[nchildren++] = red_scale;

	XtAddCallback(red_scale, XmNdragCallback,
				  fe_color_rgb_scale_cb, (XtPointer)info);

	XtAddCallback(red_scale, XmNvalueChangedCallback,
				  fe_color_rgb_scale_cb, (XtPointer)info);

	n = 0;
	XtSetArg(args[n], XmNcolumns, TEXT_NCOLUMNS); n++;
	XtSetArg(args[n], XmNuserData, (XtPointer)XFE_COLOR_RED); n++;
	red_text = fe_CreateTextField(form, "redText", args, n);
	children[nchildren++] = red_text;

	XtAddCallback(red_text, XmNvalueChangedCallback,
				  fe_color_rgb_text_cb, (XtPointer)info);

	tmp_string = XmStringCreateSimple("G");

	n = 0;
	XtSetArg(args[n], XmNlabelType, XmSTRING); n++;
	XtSetArg(args[n], XmNlabelString, tmp_string); n++;
	green_label = XmCreateLabelGadget(form, "greenLabel", args, n);
	children[nchildren++] = green_label;
	
	XmStringFree(tmp_string);
	
	n = 0;
	XtSetArg(args[n], XmNuserData, (XtPointer)XFE_COLOR_GREEN); n++;
	XtSetArg(args[n], XmNshowValue, SHOW_VALUE); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNmaximum, 256); n++;
	green_scale = XmCreateScale(form, "greenScale", args, n);
	children[nchildren++] = green_scale;
	
	XtAddCallback(green_scale, XmNdragCallback,
				  fe_color_rgb_scale_cb, (XtPointer)info);

	XtAddCallback(green_scale, XmNvalueChangedCallback,
				  fe_color_rgb_scale_cb, (XtPointer)info);

	n = 0;
	XtSetArg(args[n], XmNuserData, (XtPointer)XFE_COLOR_GREEN); n++;
	XtSetArg(args[n], XmNcolumns, TEXT_NCOLUMNS); n++;
	green_text = fe_CreateTextField(form, "greenText", args, n);
	children[nchildren++] = green_text;

	XtAddCallback(green_text, XmNvalueChangedCallback,
				  fe_color_rgb_text_cb, (XtPointer)info);

	tmp_string = XmStringCreateSimple("B");

	n = 0;
	XtSetArg(args[n], XmNlabelType, XmSTRING); n++;
	XtSetArg(args[n], XmNlabelString, tmp_string); n++;
	blue_label = XmCreateLabelGadget(form, "blueLabel", args, n);
	children[nchildren++] = blue_label;
	
	XmStringFree(tmp_string);
	
	n = 0;
	XtSetArg(args[n], XmNuserData, (XtPointer)XFE_COLOR_BLUE); n++;
	XtSetArg(args[n], XmNshowValue, SHOW_VALUE); n++;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNmaximum, 256); n++;
	blue_scale = XmCreateScale(form, "blueScale", args, n);
	children[nchildren++] = blue_scale;

	XtAddCallback(blue_scale, XmNdragCallback,
				  fe_color_rgb_scale_cb, (XtPointer)info);

	XtAddCallback(blue_scale, XmNvalueChangedCallback,
				  fe_color_rgb_scale_cb, (XtPointer)info);

	n = 0;
	XtSetArg(args[n], XmNuserData, (XtPointer)XFE_COLOR_BLUE); n++;
	XtSetArg(args[n], XmNcolumns, TEXT_NCOLUMNS); n++;
	blue_text = fe_CreateTextField(form, "blueText", args, n);
	children[nchildren++] = blue_text;

	XtAddCallback(blue_text, XmNvalueChangedCallback,
				  fe_color_rgb_text_cb, (XtPointer)info);

	XtManageChildren(children, nchildren);

	/*
	 *    Do units option menu.
	 */
	n = 0;
	pulldown = fe_CreatePulldownMenu(form, "unitsMenu", args, n);

	nchildren = 0;

	n = 0;
	XtSetArg(args[n], XmNuserData, (XtPointer)0); n++;
	button = XmCreatePushButtonGadget(pulldown, "decimal", args, n);
	XtAddCallback(button, XmNactivateCallback, fe_color_rgb_units_cb,
				  (XtPointer)info);
	children[nchildren++] = button;

	n = 0;
	XtSetArg(args[n], XmNuserData, (XtPointer)1); n++;
	button = XmCreatePushButtonGadget(pulldown, "hex", args, n);
	XtAddCallback(button, XmNactivateCallback, fe_color_rgb_units_cb,
				  (XtPointer)info);
	children[nchildren++] = button;

	n = 0;
	XtSetArg(args[n], XmNuserData, (XtPointer)2); n++;
	button = XmCreatePushButtonGadget(pulldown, "percent", args, n);
	XtAddCallback(button, XmNactivateCallback, fe_color_rgb_units_cb,
				  (XtPointer)info);
	children[nchildren++] = button;

	XtManageChildren(children, nchildren);

	n = 0;
	XtSetArg(args[n], XmNsubMenuId, pulldown); n++;
	units = XmCreateOptionMenu(form, "units", args, n);
	XtManageChild(units);
	
	/*
	 *    Now do attachments! We actually connect up from the
	 *    bottom.
	 */
	n = 0;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNleftWidget, red_scale); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, red_scale); n++;
	XtSetValues(min_label, args, n);

	n = 0;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_OPPOSITE_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, red_scale); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNbottomWidget, red_scale); n++;
	XtSetValues(max_label, args, n);

	for (i = 0; i < 3; i++) {

		if (i == 0) {
			bottom = green_text;
			text = red_text;
			scale = red_scale;
			label = red_label;
		} else if (i == 1) {
			bottom = blue_text;
			text = green_text;
			scale = green_scale;
			label = green_label;
		} else {
			bottom = units;
			text = blue_text;
			scale = blue_scale;
			label = blue_label;
		}

		n = 0;
		XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
		XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(args[n], XmNbottomWidget, bottom); n++;
		XtSetValues(text, args, n);

		n = 0;
		XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
		XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(args[n], XmNbottomWidget, bottom); n++;
		XtSetValues(label, args, n);

		n = 0;
		XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(args[n], XmNrightWidget, text); n++;
		XtSetArg(args[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(args[n], XmNleftWidget, label); n++;
		XtSetArg(args[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
		XtSetArg(args[n], XmNbottomWidget, bottom); n++;
		XtSetValues(scale, args, n);
	}

	n = 0;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetValues(units, args, n);

	info->rgb.max = max_label;
	info->rgb.red_scale = red_scale;
	info->rgb.green_scale = green_scale;
	info->rgb.blue_scale = blue_scale;

	info->rgb.red_text = red_text;
	info->rgb.green_text = green_text;
	info->rgb.blue_text = blue_text;

	info->rgb.units = units;

	return rgb_frame;
}

static Widget
fe_create_sample_group(Widget parent, fe_SamplePageInfo* info)
{
	Widget frame;
	Widget row;
	Widget old_sample;
	Widget new_sample;
	Cardinal n;
	Arg args[8];

	n = 0;
	XtSetArg(args[n], XmNorientation, XmHORIZONTAL); n++;
	XtSetArg(args[n], XmNpacking, XmPACK_COLUMN); n++;
	row = XmCreateRowColumn(parent, "sample", args, n);
	XtManageChild(row);
	frame = row;

	n = 0;
	old_sample = XmCreateDrawnButton(row, "oldSample", args, n);
	XtManageChild(old_sample);

	n = 0;
	new_sample = fe_CreateSwatch(row, "newSample", args, n);
	XtManageChild(new_sample);

	info->sample.old = old_sample;
	info->sample.new = new_sample;

	return frame;
}

/*
 *    rgb buttons - unused.
 */
#if 0
static Widget
fe_create_rgb_buttons_group(Widget parent, fe_RgbPageInfo* info)
{
	Widget frame;
	Widget row;
	Cardinal n;
	Arg args[8];
	XmString xm_string;
	Widget grab;
	Widget match;

	/*
	 *    Standard colors: frame with N color buttons.
	 */
	n = 0;
	frame = XmCreateFrame(parent, "buttonsFrame", args, n);
	XtManageChild(frame);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	row = XmCreateRowColumn(frame, "buttons", args, n);
	XtManageChild(row);

	xm_string = XmStringCreateSimple("Dropper...");

	n = 0;
	XtSetArg(args[n], XmNlabelString, xm_string); n++;
	grab = XmCreatePushButtonGadget(row, "grab", args, n);
	XtManageChild(grab);

	XmStringFree(xm_string);

	xm_string = XmStringCreateSimple("Match");

	n = 0;
	XtSetArg(args[n], XmNlabelString, xm_string); n++;
	match = XmCreatePushButtonGadget(row, "match", args, n);
	XtManageChild(match);

	XmStringFree(xm_string);

#if 0
	Widget info_label;

	xm_string = XmStringCreateSimple("white");

	n = 0;
	XtSetArg(args[n], XmNlabelString, xm_string); n++;
	info_label = XmCreateLabelGadget(row, "info", args, n);
	XtManageChild(info_label);

	XmStringFree(xm_string);
#endif

	return frame;
}
#endif

static void
fe_color_page_destroy_cb(Widget form, XtPointer closure, XtPointer cbd)
{
#if 0
	fprintf(stderr, "detroying page %s\n", XtName(form));
#endif
	if (closure)
		XtFree((char*)closure);
}

static Widget
fe_create_rgb_page(Widget folder, char* name, fe_ColorUpdateInfo* info)
{
	Widget rgb_group;
	Widget sample_group;
	Widget form;
	Arg    args[16];
	Cardinal n;
	fe_RgbPageInfo* rgb_info = XtNew(fe_RgbPageInfo);

	rgb_info->update_info = info;

	fe_color_install_page(info,
						  (fe_ColorUpdateMethod_t)fe_color_rgb_page_update,
						  (fe_SamplePageInfo*)rgb_info);

	n = 0;
	form = fe_CreateTabForm(folder, name, args, n);

	XtAddCallback(form,
				  XmNdestroyCallback, fe_color_page_destroy_cb, rgb_info);

	sample_group = fe_create_sample_group(form, (fe_SamplePageInfo*)rgb_info);
	rgb_group = fe_create_rgb_group(form, rgb_info);
#ifdef DO_BUTTONS
	Widget buttons_group;
	buttons_group = fe_create_rgb_buttons_group(form, rgb_info);
#endif

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetValues(sample_group, args, n);

#ifdef DO_BUTTONS
	n = 0;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, sample_group); n++;
	XtSetValues(buttons_group, args, n);
#endif

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, sample_group); n++;
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
#ifdef DO_BUTTONS
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, buttons_group); n++;
#else
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
#endif
	XtSetValues(rgb_group, args, n);

	return form;
}

static void
fe_color_swatches_page_update(fe_SwatchesPageInfo* page_info,
							  Widget caller, unsigned mask)
{
	fe_ColorUpdateInfo* info = page_info->update_info;

	if ((mask & (XFE_COLOR_RED|XFE_COLOR_GREEN|XFE_COLOR_BLUE)) != 0) {

		/*
		 *    Update the sample
		 */
		fe_color_sample_part_update(&page_info->sample, info, caller, mask);
	}
}

/*
 *    reset, load, save, append buttons for matrix page. unused.
 */
#if 0
static Widget
fe_create_swatch_buttons_group(Widget parent, void* notused)
{
	Widget frame;
	Widget row;
	Cardinal n;
	Arg args[8];
	XmString xm_string;
	Widget reset;
	Widget load;
	Widget save;
	Widget append;

	/*
	 *    Standard colors: frame with N color buttons.
	 */
	n = 0;
	frame = XmCreateFrame(parent, "buttonsFrame", args, n);
	XtManageChild(frame);

	n = 0;
	XtSetArg(args[n], XmNorientation, XmVERTICAL); n++;
	row = XmCreateRowColumn(frame, "buttons", args, n);
	XtManageChild(row);

	xm_string = XmStringCreateSimple("Reset");

	n = 0;
	XtSetArg(args[n], XmNlabelString, xm_string); n++;
	reset = XmCreatePushButtonGadget(row, "reset", args, n);
	XtManageChild(reset);

	XmStringFree(xm_string);

	xm_string = XmStringCreateSimple("Load..");

	n = 0;
	XtSetArg(args[n], XmNlabelString, xm_string); n++;
	load = XmCreatePushButtonGadget(row, "load", args, n);
	XtManageChild(load);

	XmStringFree(xm_string);

	xm_string = XmStringCreateSimple("Append..");

	n = 0;
	XtSetArg(args[n], XmNlabelString, xm_string); n++;
	append = XmCreatePushButtonGadget(row, "append", args, n);
	XtManageChild(append);

	XmStringFree(xm_string);

	xm_string = XmStringCreateSimple("Save..");

	n = 0;
	XtSetArg(args[n], XmNlabelString, xm_string); n++;
	save = XmCreatePushButtonGadget(row, "save", args, n);
	XtManageChild(save);

	XmStringFree(xm_string);

	return frame;
}
#endif

static void
fe_swatches_activate_cb(Widget widget, XtPointer closure, XtPointer cbd)
{
	fe_SwatchesPageInfo* swatches_info = (fe_SwatchesPageInfo*) closure;
	fe_ColorUpdateInfo* info = swatches_info->update_info;
	XmDrawnButtonCallbackStruct* cbs = (XmDrawnButtonCallbackStruct*)cbd;
	int    colorn;
	XColor color;
	XButtonEvent* event = (XButtonEvent*)cbs->event;

	fe_SwatchMatrixGetColor(widget, event->x, event->y, &color);

	info->color.red = color.red;
	info->color.green = color.green;
	info->color.blue = color.blue;
	info->pixel = color.pixel;

	fe_color_swatches_page_update(swatches_info, widget, 
					  (XFE_COLOR_RED|XFE_COLOR_GREEN|XFE_COLOR_BLUE));
}

static Widget
fe_create_swatch_group(Widget parent, fe_SwatchesPageInfo* info)
{
	fe_SwatchesPart* swatches = &info->swatches;
	Widget frame;
	Widget matrix;
	Arg    args[16];
	Cardinal n;

	n = 0;
	frame = XmCreateFrame(parent, "swatchesFrame", args, n);
	XtManageChild(frame);

	n = 0;
	matrix = fe_CreateSwatchMatrix(frame, "swatches", args, n);

	fe_AddSwatchMatrixCallback(matrix, XmNactivateCallback, 
							   fe_swatches_activate_cb, info);

	XtManageChild(matrix);

	swatches->matrix = matrix;

	return frame;
}

static Widget
fe_create_swatch_page(Widget folder, char* name, fe_ColorUpdateInfo* info)
{
	Widget sample_group;
	Widget swatch_group;
	Widget form;
	Arg    args[16];
	Cardinal n;
	fe_SwatchesPageInfo* swatches_info = XtNew(fe_SwatchesPageInfo);

	fe_color_install_page(info,
					  (fe_ColorUpdateMethod_t)fe_color_swatches_page_update,
					  (fe_SamplePageInfo*)swatches_info);

	n = 0;
	form = fe_CreateTabForm(folder, name, args, n);

	XtAddCallback(form,
				  XmNdestroyCallback, fe_color_page_destroy_cb, swatches_info);

	sample_group = fe_create_sample_group(form, (fe_SamplePageInfo*)swatches_info);
	swatch_group = fe_create_swatch_group(form, swatches_info);
#ifdef DO_BUTTONS
	Widget buttons_group;
	buttons_group = fe_create_swatch_buttons_group(form, swatches_info);
#endif

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
	XtSetValues(sample_group, args, n);

#ifdef DO_BUTTONS
	n = 0;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, sample_group); n++;
	XtSetValues(buttons_group, args, n);
#endif

	n = 0;
	XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNtopWidget, sample_group); n++;
	XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
#ifdef DO_BUTTONS
	XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
	XtSetArg(args[n], XmNrightAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(args[n], XmNrightWidget, buttons_group); n++;
#endif
	XtSetValues(swatch_group, args, n);

	return form;
}

static void
fe_color_page_raised_cb(Widget folder, XtPointer closure, XtPointer cbd)
{
	fe_ColorUpdateInfo* info = (fe_ColorUpdateInfo*)closure;
	fe_SamplePageInfo*  page;

	info->pixel = fe_color_get_pixel(folder, 
									 info->color.red,
									 info->color.green,
									 info->color.blue);

	for (page = info->list; page != NULL; page = page->next) {
		if (page->update_method != NULL)
			(page->update_method)(page, folder, ~XFE_COLOR_INIT);
	}
}

Widget
fe_CreateColorPicker(Widget dialog, char* name, Arg* args, Cardinal nargs)
{
	Widget folder;
	Widget swatch_form;
	Widget rgb_form;
	Cardinal n;
	fe_ColorUpdateInfo* info;
	char* units;
	Widget parent = XtParent(dialog);

	info = XtNew(fe_ColorUpdateInfo); /* FIXME */
	memset(info, 0, sizeof(fe_ColorUpdateInfo));

	info->color.red = 0;
	info->color.green = 0;
	info->color.blue = 0;
	info->pixel = 0;
	info->units = XFE_COLOR_DECIMAL;
	info->list = NULL;

	units = XfeSubResourceGetStringValue(parent,
										 XtName(parent), 
										 "XfeColorPicker",
										 "rgbUnits",
										 "RgbUnits",
										 NULL);
	if (units != NULL) {
		if (strcmp(units, "percent") == 0)
			info->units = XFE_COLOR_PERCENT;
		if (strcmp(units, "hexidecimal") == 0)
			info->units = XFE_COLOR_HEX;
	}
	
	n = 0;
	folder = XtVaCreateWidget(name, xmlFolderWidgetClass, dialog,
							  XmNshadowThickness, 2,
#ifdef ALLOW_TAB_ROTATE
							  XmNtabPlacement, XmFOLDER_LEFT,
							  XmNrotateWhenLeftRight, FALSE,
#endif /* ALLOW_TAB_ROTATE */
							  XmNbottomOffset, 3,
							  XmNspacing, 1,
							  NULL);
    XtManageChild(folder);

	XtAddCallback(folder,
				  XmNactivateCallback, fe_color_page_raised_cb, info);
	XtAddCallback(folder,
				  XmNdestroyCallback, fe_color_page_destroy_cb, info);

	n = 0;
	swatch_form = fe_create_swatch_page(folder, "swatches", info);
    XtManageChild(swatch_form);
	
	n = 0;
	rgb_form = fe_create_rgb_page(folder, "rgb", info);
    XtManageChild(rgb_form);

	XtVaSetValues(folder, XmNuserData, info, 0);

	return folder;
}

void
fe_ColorPickerSetColor(Widget folder, LO_Color* color)
{
	fe_ColorUpdateInfo* info;
	fe_SamplePageInfo*  page;

	XtVaGetValues(folder, XmNuserData, &info, 0);
	
	if (info != NULL) {
		info->color.red = color->red;
		info->color.green = color->green;
		info->color.blue = color->blue;

		info->pixel = fe_color_get_pixel(folder, 
										 info->color.red,
										 info->color.green,
										 info->color.blue);

		for (page = info->list; page != NULL; page = page->next) {
			if (page->update_method != NULL)
				(page->update_method)(page, folder, ~0);
		}
	}
}

void
fe_ColorPickerGetColor(Widget folder, LO_Color* color)
{
	fe_ColorUpdateInfo* info;

	XtVaGetValues(folder, XmNuserData, &info, 0);
	
	if (info != NULL) {
		color->red = info->color.red;
		color->green = info->color.green;
		color->blue = info->color.blue;
	}
}

void
fe_ColorPickerSetActiveTab(Widget folder, fe_ColorPickerTabType tab_type)
{
	unsigned tab_number = 0;

	switch (tab_type) {
	case XFE_COLOR_PICKER_SWATCHES:
		tab_number = 0;
		break;
	case XFE_COLOR_PICKER_RGB:
		tab_number = 1;
		break;
	case XFE_COLOR_PICKER_LAST:
		tab_number = 0; /*FIXME*/
		break;
	}
	XmLFolderSetActiveTab(folder, tab_number, False);
}

Widget
fe_CreateColorPickerDialog(Widget parent, char* name,
						   Arg* args, Cardinal nargs)
{
	MWContext* context = (MWContext *)fe_WidgetToMWContext(parent);
	Widget dialog;
	Widget folder;

	/*
	 *    Make prompt with ok, cancel, no apply, no separator.
	 */
	dialog = fe_CreatePromptDialog(context, name,
								   TRUE, TRUE, FALSE, FALSE, TRUE);

	folder = fe_CreateColorPicker(dialog, "colorPicker", args, nargs);
    XtManageChild(folder);

	return dialog;
}

void
fe_ColorPickerDialogGetColor(Widget dialog, LO_Color* color)
{
	Widget folder = XtNameToWidget(dialog, "colorPicker");
	fe_ColorPickerGetColor(folder, color);
}

void
fe_ColorPickerDialogSetColor(Widget dialog, LO_Color* color)
{
	Widget folder = XtNameToWidget(dialog, "colorPicker");
	fe_ColorPickerSetColor(folder, color);
}

void
fe_ColorPickerDialogSetActiveTab(Widget dialog, fe_ColorPickerTabType tab_type)
{
	Widget folder = XtNameToWidget(dialog, "colorPicker");
	fe_ColorPickerSetActiveTab(folder, tab_type);
}
