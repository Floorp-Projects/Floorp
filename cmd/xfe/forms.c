/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   forms.c --- creating and displaying forms widgets
   Created: Jamie Zawinski <jwz@netscape.com>, 21-Jul-94.
   Whacked into a completely new form: Chris Toshok <toshok@netscape.com>, 29-Oct-1997.
 */


#include "mozilla.h"
#include "xfe.h"
#include "fonts.h"
#include "felocale.h"
#include "xpform.h"
#include "layers.h"

#include <plevent.h>		/* for mocha */
#include <prtypes.h>
#include <libevent.h>

#include <libi18n.h>
#include "intl_csi.h"
#include "fonts.h"
#ifndef NO_WEB_FONTS
#include "nf.h"
#include "Mnfrc.h"
#include "Mnfrf.h"
#endif

#include "DtWidgets/ComboBoxP.h"

#include <Xfe/XfeP.h>			/* for xfe widgets and utilities */

/* XXX */
/* #define NO_NEW_DEFAULT_SIZES 1 */

/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_ERROR_OPENING_FILE;

/* Kludge around conflicts between Motif and xp_core.h... */
#undef Bool
#define Bool char

/* #define FORMS_ARE_FONTIFIED */
/* #define FORMS_ARE_COLORED */


/* Kludge kludge kludge, should be in resources... */
#define fe_FORM_LEFT_MARGIN   0
#define fe_FORM_RIGHT_MARGIN  0
#define fe_FORM_TOP_MARGIN    4
#define fe_FORM_BOTTOM_MARGIN 4

static void fe_form_file_browse_cb (Widget, XtPointer, XtPointer);
static void fe_activate_submit_cb (Widget, XtPointer, XtPointer);
static void fe_submit_form_cb (Widget, XtPointer, XtPointer);
static void fe_reset_form_cb (Widget, XtPointer, XtPointer);
static void fe_radio_form_cb (Widget, XtPointer, XtPointer);
static void fe_check_form_cb (Widget, XtPointer, XtPointer);
static void fe_button_form_cb (Widget, XtPointer, XtPointer);
static void fe_combo_form_cb(Widget, XtPointer, XtPointer);
static void fe_got_focus_cb (Widget, XtPointer, XtPointer);
static void fe_list_form_cb (Widget, XtPointer, XtPointer);
static void fe_lost_focus_cb (Widget, XtPointer, XtPointer);
static void fe_mocha_focus_notify_eh (Widget w, 
			  XtPointer closure, 
			  XEvent *ev, 
			  Boolean *cont);

static void fe_key_handler(Widget w, XtPointer client_data, XEvent * xevent, Boolean * bcontinue);
static void fe_complete_js_event(JSEvent *js_event, XEvent *x_event, XP_Bool do_geometry);

#ifdef DEBUG
static char * private_precondition_format = "debug: precondition violation at %s:%d\n";
static char * private_check_format = "debug: check violation at %s:%d\n";
#endif

typedef struct fe_form_vtable FEFormVtable;
typedef struct fe_form_data FEFormData;

struct fe_form_vtable
{
  void (*create_widget_func)(FEFormData *, LO_FormElementStruct *);
  void (*get_size_func)(FEFormData *, LO_FormElementStruct *);
  void (*element_is_submit_func)(FEFormData *, LO_FormElementStruct *);
  void (*display_element_func)(FEFormData *, LO_FormElementStruct  *);
  void (*get_element_value)(FEFormData *, LO_FormElementStruct  *, XP_Bool);
  void (*free_element_func)(FEFormData *, LO_FormElementData *);
  void (*reset_element)(FEFormData *, LO_FormElementStruct *);
  void (*select_element_func)(FEFormData *, LO_FormElementStruct *);
  void (*change_element_func)(FEFormData *, LO_FormElementStruct *);
  void (*focus_element_func)(FEFormData *, LO_FormElementStruct *);
  void (*lost_focus_func)(FEFormData *);
};

struct fe_form_data {
  FEFormVtable vtbl;
  LO_FormElementStruct *form;
  Widget widget;
  MWContext *context;
};

typedef struct {
  FEFormData form_data;

  Widget rowcolumn;
  Widget browse_button;
  Widget file_widget;
} FEFileFormData;

typedef struct {
  FEFormData form_data;

  Widget text_widget; /* form_data.widget is the scrolled window. */
} FETextAreaFormData;

typedef struct {
  FEFormData form_data;

  Widget list_widget; /* form_data.widget is the scrolled window. */
  int nkids;
  char *selected_p;
} FESelectMultFormData;

typedef FESelectMultFormData FESelectOneFormData;

static FEFormData* alloc_form_data(int32 type);

static void text_create_widget(FEFormData *, LO_FormElementStruct *);
static void text_display(FEFormData *, LO_FormElementStruct *);
static void text_get_value(FEFormData *, LO_FormElementStruct *, XP_Bool);
static void text_reset(FEFormData *, LO_FormElementStruct *);
static void text_change(FEFormData *, LO_FormElementStruct *);
static void text_focus(FEFormData *, LO_FormElementStruct *);
static void text_lost_focus(FEFormData *);
static void text_select(FEFormData *, LO_FormElementStruct *);

static void file_create_widget(FEFormData *, LO_FormElementStruct *);
static void file_get_value(FEFormData *, LO_FormElementStruct *, XP_Bool);
static void file_free(FEFormData *, LO_FormElementData *);

static void button_create_widget(FEFormData *, LO_FormElementStruct *);

static void checkbox_create_widget(FEFormData *, LO_FormElementStruct *);
static void checkbox_get_value(FEFormData *, LO_FormElementStruct *, XP_Bool);
static void checkbox_change(FEFormData *, LO_FormElementStruct *);

static void select_create_widget(FEFormData *, LO_FormElementStruct *);
static void select_get_value(FEFormData *, LO_FormElementStruct *, XP_Bool);
static void select_free(FEFormData *, LO_FormElementData *);
static void select_reset(FEFormData *, LO_FormElementStruct *);
static void select_change(FEFormData *, LO_FormElementStruct *);

static void textarea_create_widget(FEFormData *, LO_FormElementStruct *);
static void textarea_display(FEFormData *, LO_FormElementStruct *);
static void textarea_get_value(FEFormData *, LO_FormElementStruct *, XP_Bool);
static void textarea_reset(FEFormData *, LO_FormElementStruct *);
static void textarea_lost_focus(FEFormData *);

static void form_element_display(FEFormData *, LO_FormElementStruct *);
static void form_element_get_size(FEFormData *, LO_FormElementStruct *);
static void form_element_is_submit(FEFormData *, LO_FormElementStruct *);
static void form_element_get_value(FEFormData *, LO_FormElementStruct *,
				   XP_Bool);
static void form_element_free(FEFormData *, LO_FormElementData *);


/* doubles as both the text and password vtable */
static FEFormVtable text_form_vtable = {
  text_create_widget,
  form_element_get_size,
  form_element_is_submit,
  text_display,
  text_get_value,
  form_element_free,
  text_reset,
  text_select,
  text_change,
  text_focus,
  text_lost_focus
};

static FEFormVtable file_form_vtable = {
  file_create_widget,
  form_element_get_size,
  form_element_is_submit,
  text_display,
  file_get_value,
  file_free,
  text_reset,
  text_select,
  text_change,
  text_focus,
  text_lost_focus
};

static FEFormVtable button_form_vtable = {
  button_create_widget,
  form_element_get_size,
  NULL,
  form_element_display,
  form_element_get_value,
  form_element_free,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
};

static FEFormVtable checkbox_form_vtable = {
  checkbox_create_widget,
  form_element_get_size,
  NULL,
  form_element_display,
  checkbox_get_value,
  form_element_free,
  NULL,
  NULL,
  checkbox_change,
  NULL,
  NULL
};

static FEFormVtable selectone_form_vtable = {
  select_create_widget,
  form_element_get_size,
  NULL,
  form_element_display,
  select_get_value,
  select_free,
  select_reset,
  NULL,
  select_change,
  NULL,
  NULL
};

static FEFormVtable selectmult_form_vtable = {
  select_create_widget,
  form_element_get_size,
  NULL,
  form_element_display,
  select_get_value,
  select_free,
  select_reset,
  NULL,
  select_change,
  NULL,
  NULL
};

static FEFormVtable textarea_form_vtable = {
  textarea_create_widget,
  form_element_get_size,
  NULL,
  textarea_display,
  textarea_get_value,
  form_element_free,
  textarea_reset,
  text_select,
  text_change,
  text_focus,
  textarea_lost_focus
};

/*
** Why are these next two functions in this file?
*/

/*
 * Raise the window to the top of the view order
 */
void
FE_RaiseWindow(MWContext *context)
{
#ifdef DEBUG_username
  printf ("RaiseWindow: context 0x%x\n", context);
#endif
  XtRealizeWidget (CONTEXT_WIDGET (context));
  XtManageChild (CONTEXT_WIDGET (context));
  /* XXX Uniconify the window if it was iconified */
  XMapRaised(XtDisplay(CONTEXT_WIDGET(context)),
	       XtWindow(CONTEXT_WIDGET(context)));
  XtPopup(CONTEXT_WIDGET(context),XtGrabNone);
}

/*
 * Lower the window to the bottom of the view order
 */
void
FE_LowerWindow(MWContext *context)
{
#ifdef DEBUG_username
  printf ("LowerWindow: context 0x%x\n", context);
#endif
  if (!XtIsRealized(CONTEXT_WIDGET(context)))
      return;

  XLowerWindow(XtDisplay(CONTEXT_WIDGET(context)),
	       XtWindow(CONTEXT_WIDGET(context)));
}

static void
fe_font_list_metrics(MWContext *context, LO_FormElementStruct *form,
		     XmFontList font_list, int *ascentp, int *descentp)
{
	fe_Font		font;
	LO_TextAttr	*text = XP_GetFormTextAttr(form);

	font = fe_LoadFontFromFace(context,text, &text->charset,
				   text->font_face,
				   text->size, text->fontmask);
	if (!font)
	{
		*ascentp = 14;
		*descentp = 3;
		return;
	}
	FE_FONT_EXTENTS(text->charset, font, ascentp, descentp);
}

static fe_Font fe_LoadFontForWidgetUnicodePseudoFont (MWContext *context,
                                     Widget parent,
				     LO_TextAttr *text_attr,
				     XmFontList *fontlist,
                                     XmFontType *type,
                                     int mask
)
{
  fe_Font fe_font;

  fe_font = fe_LoadUnicodeFont(NULL, "", 0, text_attr->size, mask,
				   0, 0, 0, XtDisplay(parent));

  /* 
     This looks very weired , 
     but it looks like this before I split the code 
   */

   *fontlist = NULL; 
   return fe_font;
}

static fe_Font fe_LoadFontForWidgetLocale (MWContext *context,
                                     Widget parent,
				     LO_TextAttr *text_attr,
				     XmFontList *fontlist,
                                     XmFontType *type,
                                     int mask
)
{
  fe_Font fe_font;
  XmFontListEntry flentry;
  XmFontList locale_font_list = NULL;

  fe_font = fe_GetFontOrFontSetFromFace(context, NULL,
				    text_attr->font_face,
			    text_attr->size,
				    mask, type);
  XP_ASSERT(NULL != fe_font);
  if (!fe_font)
	return NULL;

  flentry = XmFontListEntryCreate(XmFONTLIST_DEFAULT_TAG, *type, fe_font);
  XP_ASSERT(NULL != flentry);
  if (!flentry)
	return NULL;

  locale_font_list = XmFontListAppendEntry(NULL, flentry);

  XP_ASSERT(NULL != locale_font_list);
  if (!locale_font_list)
	return NULL;

  XmFontListEntryFree(&flentry);
      
  *fontlist = locale_font_list;
  return fe_font;
}

static void
fe_FontlistAndXmStringForOptions( MWContext *context,
				     LO_TextAttr *text_attr,
					 LO_FormElementData *form_data,
					 int nitems,
				     XmString *xmstrings,
				     XmFontList *fontlist)
{
  Widget parent = CONTEXT_DATA(context)->drawing_area;
  XP_Bool use_UnicodePseudoFont = IS_UNICODE_CSID(text_attr->charset);
  fe_Font fe_font;
  XmFontType type = 0; /* keep purify happy */

  if(use_UnicodePseudoFont)
  {
      fe_font = fe_LoadFontForWidgetUnicodePseudoFont(context, parent, text_attr, fontlist, &type,
         				text_attr->fontmask);
      if(*fontlist)
         *fontlist = XmFontListCopy(*fontlist);
  }
  else
  {
      fe_font = fe_LoadFontFromFace(
                            context, text_attr, 
                            &text_attr->charset,
                            text_attr->font_face, 
                            text_attr->size,            
                            text_attr->fontmask            
      );
      FE_NONEDIT_FONTLIST(text_attr->charset, fe_font, *fontlist);
  }

  if (xmstrings)
  {
    int i;
    for(i = 0 ; i < nitems; i++)
    {
	unsigned char* text = (unsigned char*) 
	     XP_FormSelectGetOption(form_data, i)->text_value;

       	if(NULL == text) 
		text = (unsigned char*) "---\?\?\?---";

        if(use_UnicodePseudoFont)
        {
             xmstrings[i] = fe_ConvertToXmString(
				     text,
				     text_attr->charset,
				     fe_font,
				     type,
				     fontlist);
        }
        else 
        {
             xmstrings[i] = FE_NONEDIT_TO_XMSTRING(text_attr->charset,
                                           fe_font,
                                           text,
                                           XP_STRLEN(text)
                                           );
        }
    }
  }
}
static void
fe_FontlistAndXmStringForButton(MWContext *context,
				     int32 form_type,
				     char *string,
				     LO_TextAttr *text_attr,
				     XmString *xmstring,
				     XmFontList *fontlist)
{
  Widget parent = CONTEXT_DATA(context)->drawing_area;
  XP_Bool use_UnicodePseudoFont;
  fe_Font fe_font;
  XmFontType type = 0; /* keep purify happy */

  use_UnicodePseudoFont = IS_UNICODE_CSID(text_attr->charset);

  if (xmstring)
	*xmstring = NULL;

  if( use_UnicodePseudoFont )
  {
      fe_font = fe_LoadFontForWidgetUnicodePseudoFont(context, parent, 
                                    text_attr, fontlist, &type,
                                    text_attr->fontmask);
  
      XP_ASSERT(NULL != fe_font);

      if (string && xmstring)
        *xmstring = fe_ConvertToXmString((unsigned char*)string,
				     text_attr->charset,
				     fe_font,
				     type,
				     fontlist);
      if(*fontlist)
         *fontlist = XmFontListCopy(*fontlist);
  }
  else 
  {
      fe_font = fe_LoadFontFromFace(
                            context, text_attr, 
                            &text_attr->charset,
                            text_attr->font_face, 
                            text_attr->size,            
                            text_attr->fontmask            
      );
      FE_NONEDIT_FONTLIST(text_attr->charset, fe_font, *fontlist);

      if (string && xmstring)
      {
         *xmstring = FE_NONEDIT_TO_XMSTRING(text_attr->charset,
                                           fe_font,
                                           string,
                                           XP_STRLEN(string)
                                           );
      }
  }
}
static void
fe_FontlistAndXmStringForFormElement(MWContext *context,
				     int32 form_type,
				     char *string,
				     LO_TextAttr *text_attr,
				     XmString *xmstring,
				     XmFontList *fontlist)
{
  Widget parent = CONTEXT_DATA(context)->drawing_area;
  XP_Bool use_UnicodePseudoFont;
  fe_Font fe_font;
  XmFontType type = 0; /* keep purify happy */
  int mask = text_attr->fontmask;

  use_UnicodePseudoFont = IS_UNICODE_CSID(text_attr->charset);

#ifdef DEBUG_username
  printf ("fe_FontlistAndXmStringForFormElement(%s)\n", string);
#endif

  switch (form_type)
    {
    case FORM_TYPE_FILE:
    case FORM_TYPE_TEXT:
    case FORM_TYPE_PASSWORD:
    case FORM_TYPE_TEXTAREA:
    case FORM_TYPE_READONLY:
      /* Make sure text fields are non-bold, non-italic, fixed. */
      mask &= (~ (LO_FONT_BOLD|LO_FONT_ITALIC));
      mask |= LO_FONT_FIXED;
      use_UnicodePseudoFont = FALSE; /* XmText doesn't use XmString */
      break;
    default:
      break;
    }

  if (xmstring)
	*xmstring = NULL;

  if(use_UnicodePseudoFont)
  	fe_font = fe_LoadFontForWidgetUnicodePseudoFont(context, parent, text_attr, fontlist, &type, mask);
  else
  	fe_font = fe_LoadFontForWidgetLocale(context, parent, text_attr, fontlist, &type, mask);
  
  XP_ASSERT(NULL != fe_font);

  if (string && xmstring)
    *xmstring = fe_ConvertToXmString((unsigned char*)string,
				     text_attr->charset,
				     fe_font,
				     type,
				     fontlist);
}
static void
set_form_colors(FEFormData *fed,
		LO_Color *lo_fg,
		LO_Color *lo_bg)
{
  Widget parent = CONTEXT_DATA (fed->context)->drawing_area;
  Pixel fg, bg;

  fg = fe_GetPixel (fed->context, lo_fg->red, lo_fg->green, lo_fg->blue);
  bg = fe_GetPixel (fed->context, lo_bg->red, lo_bg->green, lo_bg->blue);
  
  if (fg == bg)
	{
	  /* the foreground and background are the same.  Try to
	     choose something that will make things readable 
	     (if a little ugly) */
	  
	  XmGetColors(XtScreen(parent),
		      XfeColormap(parent),
		      bg,
		      &fg, NULL, NULL, NULL);
	}

  XtVaSetValues(fed->widget,
		XmNforeground, fg,
		XmNbackground, bg,
		NULL);
}

static void
get_form_colors(FEFormData *fed,
		LO_Color *lo_fg,
		LO_Color *lo_bg,
		Pixel *fg_r, Pixel *bg_r)
{
  Widget parent = CONTEXT_DATA (fed->context)->drawing_area;
  Pixel fg, bg;

  fg = fe_GetPixel (fed->context, lo_fg->red, lo_fg->green, lo_fg->blue);
  bg = fe_GetPixel (fed->context, lo_bg->red, lo_bg->green, lo_bg->blue);
  
  if (fg == bg)
	{
	  /* the foreground and background are the same.  Try to
	     choose something that will make things readable 
	     (if a little ugly) */
	  
	  XmGetColors(XtScreen(parent),
		      XfeColormap(parent),
		      bg,
		      &fg, NULL, NULL, NULL);
	}

  *fg_r = fg;
  *bg_r = bg;
}

/* Motif likes to complain when there are binary characters in
   text strings.
 */
void
fe_forms_clean_text (MWContext *context,
		     int charset,
		     char *text,
		     Boolean newlines_too_p)
{
  unsigned char *t = (unsigned char *) text;
  for (; *t; t++)
    {
      unsigned char c = *t;
      if ((newlines_too_p && (c == '\n' || c == '\r')) ||
	  (c < ' ' && c != '\t' && c != '\n' && c != '\r') ||
	  (c == 0x7f) ||
	  ((INTL_CharSetType(charset) == SINGLEBYTE) &&
	   (0x7f <= c) && (c <= 0xa0)))
	*t = ' ';
    }
}

static void fe_HackTextTranslations_addCallbacks(Widget widget)
{
#if 0
  /* make sure it gets added only once */
  XtRemoveCallback(widget,
		   XmNmodifyVerifyCallback,
		   fe_textModifyVerifyCallback,
		   NULL);
  XtRemoveCallback(widget,
		   XmNmotionVerifyCallback,
		   fe_textMotionVerifyCallback,
		   NULL);
#endif
  XtAddCallback(widget,
		XmNmotionVerifyCallback,
		fe_textMotionVerifyCallback,
		NULL);
  XtAddCallback(widget,
		XmNmodifyVerifyCallback,
		fe_textModifyVerifyCallback,
		NULL);
  XtVaSetValues(widget,
		XmNverifyBell,False,
		NULL);
}

/*
 * A list key event handler.
 * This will search for a key in the XmList and shift the location cursor
 * to a match.
 */
static void
fe_list_find_eh(Widget widget, XtPointer closure, XEvent* event,
		Boolean* continue_to_dispatch)
{
  MWContext *context = (MWContext *) closure;
  Boolean usedEvent_p = False;

  Modifiers mods;
  KeySym k;
  char *s;
  int i;
  int index;				/* index is 1 based */
  XmString *items;
  int nitems;

#define MILLI_500_SECS	500
#define _LIST_MAX_CHARS	100
  static char input[_LIST_MAX_CHARS] = {0};
  static Time prev_time;
  static Widget w = 0;
  static Boolean is_prev_found = False;

  if (!context || !event) return;

  fe_UserActivity (context);

  if (event->type == KeyPress) {
    if (!(k = XtGetActionKeysym (event, &mods))) return;
    if (!(s = XKeysymToString (k))) return;
    if (!s || !*s || s[1]) return;		/* no or more than 1 char */
    if (!isprint(*s)) return;			/* unprintable */

    if ((widget == w) && (event->xkey.time - prev_time < MILLI_500_SECS)) {
	/* Continue search */
	if (is_prev_found) strcat(input, s);
	else return;
    }
    else {
	/* Fresh search */
	is_prev_found = False;
	strcpy(input, s);
    }
    w = widget;
    prev_time = event->xkey.time;
    index = XmListGetKbdItemPos(widget);
    if (!index) return;				/* List is empty */

    /* Wrap search for input[] in List */
    i = index;
    i = (i == nitems) ? 1 : i+1;		/* Increment i */
    XtVaGetValues(widget, XmNitems, &items, XmNitemCount, &nitems, 0);
    while (i != index) {
	if (!XmStringGetLtoR(items[i-1], XmSTRING_DEFAULT_CHARSET, &s))
	    continue;
	if (!strncmp(s, input, strlen(input))) {
	    /* Found */
	    XP_FREE(s);
	    XmListSetKbdItemPos(widget, i);
	    is_prev_found = True;
	    break;
	}
	XP_FREE(s);
	i = (i == nitems) ? 1 : i+1;	/* Increment i */
    }
    usedEvent_p = True;
  }

  if (usedEvent_p) *continue_to_dispatch = False;
}

/*
**
** VTABLE for "superclass", form_element.
**
*/
static void
form_element_display(FEFormData *fed,
		     LO_FormElementStruct *form)
{
  MWContext *context = fed->context;
  Arg av[10];
  int ac;
  uint16 attrmask = XP_GetFormEleAttrmask(form);
  Position x;
  Position y;

  x = (form->x + form->x_offset
       - CONTEXT_DATA (context)->document_x);
  y = (form->y + form->y_offset
       - CONTEXT_DATA (context)->document_y);
  x += fe_FORM_LEFT_MARGIN;
  y += fe_FORM_TOP_MARGIN;

  if (!XtIsManaged(fed->widget)) 
    {
      Dimension bw = 0;
      Dimension width = form->width;
      Dimension height = form->height;

      XtVaGetValues (fed->widget, XmNborderWidth, &bw, 0);
      /* Since I told layout that width was width+bw+bw (for its sizing and
	 positioning purposes), subtract it out again when setting the size of
	 the text area.  X thinks the borders are outside the WxH rectangle,
	 not inside, but thinks that the borders are below and to the right of
	 the X,Y origin, instead of up and to the left as you would expect
	 given the WxH behavior!!
      */
      width -= (bw * 2);
      height -= (bw * 2);
      /* x += bw; */
      /* y += bw;*/
      
      width  -= fe_FORM_LEFT_MARGIN + fe_FORM_RIGHT_MARGIN;
      height -= fe_FORM_TOP_MARGIN + fe_FORM_BOTTOM_MARGIN;
      
      /*
       * Move to 0 before moving to where we want so it ends up
       * where we want.
       */
      ac = 0;
      XtSetArg (av [ac], XmNx, 0); ac++;
      XtSetArg (av [ac], XmNy, 0); ac++;
      XtSetValues (fed->widget, av, ac);
      
      
#ifdef DEBUG_username
      printf ("DisplayFormElement: width = %d; height = %d\n", width, height);
#endif
      
      ac = 0;
      XtSetArg (av [ac], XmNx, (Position)x); ac++;
      XtSetArg (av [ac], XmNy, (Position)y); ac++;
      XtSetArg (av [ac], XmNwidth, (Dimension)width); ac++;
      XtSetArg (av [ac], XmNheight, (Dimension)height); ac++;
      XtSetValues (fed->widget, av, ac);
      
      XtSetMappedWhenManaged(fed->widget,
			     !(attrmask & LO_ELE_INVISIBLE));
      
      XtManageChild (fed->widget);
    }
  else
    {
      XtSetMappedWhenManaged(fed->widget,
			     !(attrmask & LO_ELE_INVISIBLE));
      
      XtMoveWidget(fed->widget, x, y);
    }
}

static void
form_element_get_size(FEFormData *fed,
		      LO_FormElementStruct *form)
{
  MWContext *context = fed->context;
  Dimension width = 0;
  Dimension height = 0;
  Dimension bw = 0;
  Dimension st = 0;
  Dimension ht = 0;
  Dimension margin_height = 0;
  Dimension bottom = 0;
  int ascent, descent;
  XmFontList font_list;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  int32 form_type = XP_FormGetType(form_data);
  Widget element_widget;

  if (form_type == FORM_TYPE_FILE)
    element_widget = ((FEFileFormData*)fed)->rowcolumn;
  else
    element_widget = fed->widget;

  font_list = 0;
  XtVaGetValues (element_widget, XmNfontList, &font_list, 0);
  
  fe_HackTranslations (context, element_widget);

  fe_font_list_metrics (context, form, font_list, &ascent, &descent);

  XtRealizeWidget(element_widget);

  if (fe_globalData.fe_guffaw_scroll == 1)
    {
      XSetWindowAttributes attr;
      unsigned long valuemask;
      
      valuemask = CWBitGravity | CWWinGravity;
      attr.win_gravity = StaticGravity;
      attr.bit_gravity = StaticGravity;
      XChangeWindowAttributes (XtDisplay (element_widget),
			       XtWindow (element_widget),
			       valuemask, &attr);
    }

  XtVaGetValues (element_widget,
		 XmNwidth,		  &width,
		 XmNheight,		  &height,
		 XmNborderWidth,	  &bw,
		 XmNshadowThickness,	  &st,
		 XmNhighlightThickness, &ht,
		 XmNmarginHeight,	  &margin_height,
		 XmNmarginBottom,	  &bottom,
		 0);
  
#ifdef DEBUG_username
  printf ("form before: width = %d; height = %d\n", width, height);
#endif
  width  += fe_FORM_LEFT_MARGIN + fe_FORM_RIGHT_MARGIN;
  height += fe_FORM_TOP_MARGIN + fe_FORM_BOTTOM_MARGIN;
  
  /* The WxH doesn't take into account the border width; so add that in
	 for layout's benefit.  We subtract it again later. */
  width  += (bw * 2);
  height += (bw * 2);
#ifdef DEBUG_username
  printf ("form after: width = %d; height = %d\n", width, height);
#endif
  form->width  = width;
  form->height = height;
  
  /* Yeah.  Uh huh. */
  if (form_type == FORM_TYPE_RADIO ||
      form_type == FORM_TYPE_CHECKBOX)
    {
      bottom = 0;
      descent = bw;
    }
  
  form->baseline = height - (bottom + margin_height + st + bw + ht + descent
			     + fe_FORM_BOTTOM_MARGIN);
}

static void
form_element_is_submit(FEFormData *fed,
		       LO_FormElementStruct *form)
{
  XtRemoveCallback (fed->widget,
		    XmNactivateCallback, fe_activate_submit_cb, fed);
  
  XtAddCallback (fed->widget,
		 XmNactivateCallback, fe_activate_submit_cb, fed);
  
  if (fe_globalData.terminal_text_translations)
    XtOverrideTranslations (fed->widget,
			    fe_globalData.terminal_text_translations);
}

static void
form_element_get_value(FEFormData *fed,
		       LO_FormElementStruct *form,
		       XP_Bool delete_p)
{
  if (delete_p)
    {
      XtDestroyWidget(fed->widget);
      fed->widget = 0;
    }
}

static void
form_element_free(FEFormData *fed,
		  LO_FormElementData *form)
{
  if (fed->widget)
    XtDestroyWidget(fed->widget);

  fed->widget = 0;

  XP_FREE(fed);
}

/*
**
** VTABLE for Text like form elements (TEXT, TEXTAREA, and FILE)
**
*/
static void
text_create_widget(FEFormData *fed,
		   LO_FormElementStruct *form)
{
  MWContext *context = fed->context;
  Widget parent;
  int16 charset;
  LO_FormElementData *form_data;
  int32 form_type;
  int32 text_size;
  int32 max_size;
  int columns;
  char *tmp_text = 0;
  char *text;
  unsigned char *loc;
  XmFontList font_list;
  Arg av [50];
  int ac;
  LO_TextAttr *text_attr;

  parent = CONTEXT_DATA (context)->drawing_area;
  form_data = XP_GetFormElementData(form);
  form_type = XP_FormGetType(form_data);
  text_size = XP_FormTextGetSize(form_data);
  max_size = XP_FormTextGetMaxSize(form_data);
  text_attr = XP_GetFormTextAttr(form);
  text = (char*)XP_FormGetDefaultText(form_data);
  charset = text_attr->charset;

  /* from bstell: in multibyte (asian) locales, characters tend to be twice
     as wide as in non-multibyte locales.  To make things look correct,
     we divide the column number in half in those cases. */
  columns = ((fe_LocaleCharSetID & MULTIBYTE) ?
	     (text_size + 1) / 2 : text_size);

  /* the default size for text fields is 20 columns. 
     XX Shouldn't the backend just fill it in with 20? */
  if (columns == 0) columns = 20;


  if (!text) text = "";

  if (max_size > 0 && (int) XP_STRLEN (text) >= max_size) {
	tmp_text = XP_STRDUP (text);
	tmp_text [max_size] = '\0';
	text = tmp_text;
  }

  fe_forms_clean_text (context, charset, text, True);

  fe_FontlistAndXmStringForFormElement(context,
				       form_type,
				       text,
				       text_attr,
				       NULL,
				       &font_list);
  if (!font_list)
	return;

  ac = 0;
  /* Ok, let's try using the fixed font for text areas only. */
/*#ifdef FORMS_ARE_FONTIFIED*/
  XtSetArg (av [ac], XmNfontList, font_list); ac++;
/*#endif*/
  XtSetArg (av [ac], XmNcolumns, columns); ac++;

  if (form_type == FORM_TYPE_READONLY)
    {
      XtSetArg (av [ac], XmNeditable, False); ac++;
      XtSetArg (av [ac], XmNcursorPositionVisible, False); ac++;
    }

  if (max_size > 0)
    {
      XtSetArg (av [ac], XmNmaxLength, max_size); ac++;
    }

  fed->widget = fe_CreateTextField (parent, "textForm", av, ac);

  /*#ifdef FORMS_ARE_COLORED*/
  if (form_type == FORM_TYPE_READONLY)
    set_form_colors(fed, &text_attr->fg, &text_attr->bg);
  /*#endif*/

  /* Install in javascript form elemement text field translation */
  fe_HackFormTextTranslations(fed->widget);

  XtAddCallback(fed->widget, XmNlosingFocusCallback,
				fe_lost_focus_cb, fed);
  XtAddCallback(fed->widget, XmNfocusCallback, fe_got_focus_cb, fed);


  /* This is changed later for self-submitting fields. */
  if (fe_globalData.nonterminal_text_translations)
	XtOverrideTranslations (fed->widget, fe_globalData.
				nonterminal_text_translations);

  if (form_type == FORM_TYPE_PASSWORD)
    {
      fe_SetupPasswdText (fed->widget, (max_size > 0 ? max_size : 1024));
      fe_MarkPasswdTextAsFormElement(fed->widget);
    }

  /* Must be after passwd setup. */
  XtVaSetValues (fed->widget, XmNcursorPosition, 0, 0);
  loc = fe_ConvertToLocaleEncoding (charset, (unsigned char *)text);
  /* Add verify callbacks to implement JS key events */
  fe_HackTextTranslations_addCallbacks(fed->widget);
  /*
   * I don't know whether it is SGI or some version of Motif,but
   * doing a XtVaSetValues() here will execute the
   * valueChangedCallback but ignore its changes.
   * XmTextFieldSetString() works as expected.
   */
  XmTextFieldSetString (fed->widget, loc);
  if (((char *) loc) != text) {
    XP_FREE (loc);
  }
  if (tmp_text) XP_FREE (tmp_text);
}

static void
text_display(FEFormData *fed,
	     LO_FormElementStruct *form)
{
  MWContext *context = fed->context;
  Widget widget_to_manage;
  uint16 attrmask = XP_GetFormEleAttrmask(form);
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  Position x;
  Position y;
  Arg av[10];
  int ac = 0;
  unsigned char *loc = 0;
  char *current_text = (char*)XP_FormGetCurrentText(form_data);
  int32 max_size = XP_FormTextGetMaxSize(form_data);
  int32 form_type = XP_FormGetType(form_data);
  LO_TextAttr *text_attr = XP_GetFormTextAttr(form);
  uint16 charset = text_attr->charset;

  x = (form->x + form->x_offset
       - CONTEXT_DATA (context)->document_x);
  y = (form->y + form->y_offset
       - CONTEXT_DATA (context)->document_y);
  x += fe_FORM_LEFT_MARGIN;
  y += fe_FORM_TOP_MARGIN;

  if (form_type == FORM_TYPE_FILE)
    widget_to_manage = ((FEFileFormData*)fed)->rowcolumn;
  else
    widget_to_manage = fed->widget;

  if (!XtIsManaged(widget_to_manage))
    {
      Dimension bw = 0;
      Dimension width = form->width;
      Dimension height = form->height;

      XtVaGetValues (widget_to_manage, XmNborderWidth, &bw, 0);
      /* Since I told layout that width was width+bw+bw (for its sizing and
	 positioning purposes), subtract it out again when setting the size of
	 the text area.  X thinks the borders are outside the WxH rectangle,
	 not inside, but thinks that the borders are below and to the right of
	 the X,Y origin, instead of up and to the left as you would expect
	 given the WxH behavior!!
       */
      width -= (bw * 2);
      height -= (bw * 2);
      /* x += bw; */
      /* y += bw;*/

      width  -= fe_FORM_LEFT_MARGIN + fe_FORM_RIGHT_MARGIN;
      height -= fe_FORM_TOP_MARGIN + fe_FORM_BOTTOM_MARGIN;

      if (max_size > 0)
	{
	  XtSetArg (av [ac], XmNmaxLength, max_size); ac++;
	}
      
      if (form_type == FORM_TYPE_TEXT) {
	if (current_text) {
	  loc = fe_ConvertToLocaleEncoding (charset,
					    (unsigned char *)current_text);
	  XtSetArg (av[ac], XmNvalue, loc);  ac++;
	}
      }

      if (ac)
	XtSetValues(widget_to_manage, av, ac);

      ac = 0;
      XtSetArg (av [ac], XmNx, x); ac++;
      XtSetArg (av [ac], XmNy, y); ac++;
      XtSetArg (av [ac], XmNwidth, width); ac++;
      XtSetArg (av [ac], XmNheight, height); ac++;
      XtSetValues (widget_to_manage, av, ac);
      
      XtSetMappedWhenManaged(widget_to_manage,
			     !(attrmask & LO_ELE_INVISIBLE));
      
      XtManageChild(widget_to_manage);
    }
  else
    {
      XtSetMappedWhenManaged(widget_to_manage,
			     !(attrmask & LO_ELE_INVISIBLE));
      
      XtMoveWidget(widget_to_manage, x, y);
    }
}

static void
text_get_value(FEFormData *fed,
	       LO_FormElementStruct *form,
	       XP_Bool delete_p)
{
  MWContext *context = fed->context;
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  char *text = 0;
  int32 form_type;
  PA_Block new_current;
  PA_Block cur_current;
  PA_Block default_txt;

  form_type = XP_FormGetType(form_data);

  if (form_type == FORM_TYPE_TEXT)
    {
      /* gross, since for password type form elements, we get back a malloc'ed
	 area of memory, and for text form elements, we get XtMalloc'ed. */
      char *non_motif_text = NULL;
      XtVaGetValues (fed->widget, XmNvalue, &text, 0);
      if (text != NULL)
	{
	  non_motif_text = XP_STRDUP(text);
	  XtFree(text);
	}
      text = non_motif_text;
    }
  else if (form_type == FORM_TYPE_PASSWORD)
    text = fe_GetPasswdText (fed->widget);

  cur_current = XP_FormGetCurrentText(form_data);
  default_txt = XP_FormGetDefaultText(form_data);

  if (cur_current && cur_current != default_txt)
    free ((char*)cur_current);

  new_current = (PA_Block)fe_ConvertFromLocaleEncoding(INTL_GetCSIWinCSID(c),
						       (unsigned char *)text);
  
  XP_FormSetCurrentText(form_data, new_current);
  
  if ((char*)new_current != text)
    free (text);

  form_element_get_value(fed, form, delete_p);
}

static void
text_reset(FEFormData *fed,
	   LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  char *default_text = (char*)XP_FormGetDefaultText(form_data);
  LO_TextAttr *text_attr = XP_GetFormTextAttr(form);
  char *tmp_text = 0;
  unsigned char *loc;
  int16 charset = text_attr->charset;
  Widget w = fed->widget;
  int32 max_size = XP_FormTextGetMaxSize(form_data);
  int32 form_type;

  form_type = XP_FormGetType(form_data);

  if (!default_text) default_text = "";

  if (max_size > 0 && (int) XP_STRLEN (default_text) >= max_size) 
    {
      tmp_text = XP_STRDUP (default_text);
      tmp_text [max_size] = '\0';
      default_text = tmp_text;
    }

  fe_forms_clean_text (fed->context, charset, default_text, True);

  XtVaSetValues (w, XmNcursorPosition, 0, 0);

  loc = fe_ConvertToLocaleEncoding (charset, (unsigned char*)default_text);
  /*
   * I don't know whether it is SGI or some version of Motif,
   * but doing a XtVaSetValues() here will execute the
   * valueChangedCallback but ignore its changes.
   * XmTextFieldSetString() works as expected.
   */
  XmTextFieldSetString (w, loc);
  if (((char *) loc) != default_text)
    {
      XP_FREE (loc);
    }

  if (tmp_text) XP_FREE (tmp_text);
}

static void
text_change(FEFormData *fed,
	    LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  int32 form_type = XP_FormGetType(form_data);
  char *text = (char*)XP_FormGetCurrentText(form_data);
  LO_TextAttr *text_attr = XP_GetFormTextAttr(form);
  unsigned char *loc;
  int16 charset = text_attr->charset;

  if (!text) text = (char*)XP_FormGetDefaultText(form_data);
  loc = fe_ConvertToLocaleEncoding(charset, (unsigned char*)text);

  if (form_type == FORM_TYPE_TEXTAREA)
    {
      FETextAreaFormData *ta_fed = (FETextAreaFormData*)fed;
      XmTextSetString(ta_fed->text_widget, loc);
    }
  else
    {
      XmTextFieldSetString(fed->widget, loc);
    }

  if (((char *) loc) != text)
    {
      XP_FREE (loc);
    }
}

static void
text_select(FEFormData *fed,
	    LO_FormElementStruct *form)
{
  XmTextSetSelection (fed->widget, 0,
		      XmTextGetMaxLength (fed->widget), CurrentTime);
}

static void
text_focus(FEFormData *fed,
	   LO_FormElementStruct *form)
{
  XmProcessTraversal(fed->widget, XmTRAVERSE_CURRENT);
}

static void
text_lost_focus(FEFormData *fed)
{
  LO_FormElementData *form_data;
  char *text;
  XP_Bool text_changed = False;
  int32 form_type;
  PA_Block current_text;
  JSEvent *event;

  form_data = XP_GetFormElementData(fed->form);
  form_type = XP_FormGetType(form_data);

  if (form_type == FORM_TYPE_READONLY) return;

  current_text = XP_FormGetCurrentText(form_data);

  if (form_type == FORM_TYPE_PASSWORD)
    text = fe_GetPasswdText (fed->widget);
  else
    XtVaGetValues (fed->widget, XmNvalue, &text, 0);

  if (!current_text || XP_STRCMP((char*)current_text, text))
	text_changed = True;

  if (((char*) current_text) != text) {
	XtFree (text);
  }

  /* if the text has changed, call get_element_value to copy it into the form
     element, and send a CHANGE event to the javascript thread. */
  if (text_changed)
	{
		  (*fed->vtbl.get_element_value)(fed, fed->form, FALSE);

		  event = XP_NEW_ZAP(JSEvent);

		  event->type = EVENT_CHANGE;

		  ET_SendEvent (fed->context, (LO_Element *) fed->form, event,
						NULL, NULL);
	  }
}

/*
**
** VTABLE for the FILE form element.
**
*/
static void
file_create_widget(FEFormData *fed,
		   LO_FormElementStruct *form)
{
  FEFileFormData *filefed = (FEFileFormData*)fed;
  MWContext *context = fed->context;
  Widget parent;
  int16 charset;
  LO_FormElementData *form_data;
  int32 form_type;
  int32 text_size;
  int32 max_size;
  int columns;
  char *tmp_text = 0;
  char *text;
  unsigned char *loc;
  XmFontList font_list;
  Arg av [50];
  int ac;
  LO_TextAttr *text_attr;
  Pixel fg, bg;

  parent = CONTEXT_DATA (context)->drawing_area;
  form_data = XP_GetFormElementData(form);
  form_type = XP_FormGetType(form_data);
  text_size = XP_FormTextGetSize(form_data);
  max_size = XP_FormTextGetMaxSize(form_data);
  text_attr = XP_GetFormTextAttr(form);
  text = (char*)XP_FormGetDefaultText(form_data);
  charset = text_attr->charset;

  /* from bstell: in multibyte (asian) locales, characters tend to be twice
     as wide as in non-multibyte locales.  To make things look correct,
     we divide the column number in half in those cases. */
  columns = ((fe_LocaleCharSetID & MULTIBYTE) ?
	     (text_size + 1) / 2 : text_size);

  /* the default size for text fields is 20 columns. 
     XX Shouldn't the backend just fill it in with 20? */
  if (columns == 0) columns = 20;


  if (!text) text = "";

  if (max_size > 0 && (int) XP_STRLEN (text) >= max_size) {
	tmp_text = XP_STRDUP (text);
	tmp_text [max_size] = '\0';
	text = tmp_text;
  }

  fe_forms_clean_text (context, charset, text, True);

  fe_FontlistAndXmStringForFormElement(context,
				       form_type,
				       text,
				       text_attr,
				       NULL,
				       &font_list);
  if (!font_list)
	return;

  get_form_colors(fed, &text_attr->fg, &text_attr->bg, &fg, &bg);

  /* Create a RowColumn widget to handle both the text and its
     associated browseButton */
  ac = 0;
  XtSetArg (av [ac], XmNpacking, XmPACK_TIGHT); ac++;
  XtSetArg (av [ac], XmNorientation, XmHORIZONTAL); ac++;
  XtSetArg (av [ac], XmNnumColumns, 2); ac++;
  XtSetArg (av [ac], XmNspacing, 5); ac++;
  XtSetArg (av [ac], XmNmarginWidth, 0); ac++;
  XtSetArg (av [ac], XmNmarginHeight, 0); ac++;
  filefed->rowcolumn = XmCreateRowColumn(parent, "formRowCol", av, ac);

  ac = 0;
  /* Ok, let's try using the fixed font for text areas only. */
/*#ifdef FORMS_ARE_FONTIFIED*/
  XtSetArg (av [ac], XmNfontList, font_list); ac++;
/*#endif*/
  XtSetArg (av [ac], XmNcolumns, columns); ac++;

  if (max_size > 0)
    {
      XtSetArg (av [ac], XmNmaxLength, max_size); ac++;
    }

#ifdef FORMS_ARE_COLORED
  XtSetArg (av [ac], XmNforeground, fg); ac++;
  XtSetArg (av [ac], XmNbackground, bg); ac++;
#endif

  fed->widget = fe_CreateTextField (filefed->rowcolumn, "textForm", av, ac);

  /* Install in javascript form elemement text field translation */
  fe_HackFormTextTranslations(fed->widget);

  XtAddCallback(fed->widget, XmNlosingFocusCallback,
				fe_lost_focus_cb, fed);
  XtAddCallback(fed->widget, XmNfocusCallback, fe_got_focus_cb, fed);


  XtManageChild(fed->widget);
  
  ac = 0;
  XtSetArg (av [ac], XmNfontList, font_list); ac++;
  XtSetArg (av [ac], XmNforeground, fg); ac++;
  XtSetArg (av [ac], XmNbackground, bg); ac++;
  filefed->browse_button = XmCreatePushButton (filefed->rowcolumn,
					       "formFileBrowseButton",
					       av, ac);
  XtAddCallback(filefed->browse_button, XmNactivateCallback,
		fe_form_file_browse_cb, fed);
  XtManageChild(filefed->browse_button);

  /* This is changed later for self-submitting fields. */
  if (fe_globalData.nonterminal_text_translations)
	XtOverrideTranslations (fed->widget, fe_globalData.
				nonterminal_text_translations);

  XtVaSetValues (fed->widget, XmNcursorPosition, 0, 0);
  loc = fe_ConvertToLocaleEncoding (charset, (unsigned char *)text);
  /* Add verify callbacks to implement JS key events */
  fe_HackTextTranslations_addCallbacks(fed->widget);
  /*
   * I don't know whether it is SGI or some version of Motif
   * but doing a XtVaSetValues() here will execute the
   * valueChangedCallback but ignore its changes.
   * XmTextFieldSetString() works as expected.
   */
  XmTextFieldSetString (fed->widget, loc);
  if (((char *) loc) != text) {
    XP_FREE (loc);
  }
  if (tmp_text) XP_FREE (tmp_text);
}

static void
file_get_value(FEFormData *fed,
	       LO_FormElementStruct *form,
	       XP_Bool delete_p)
{
  FEFileFormData *filefed = (FEFileFormData*)fed;
  
  text_get_value(fed, form, delete_p);

  /* if delete_p is true, then form_element_get_value will have destroyed
     fed->widget (our text field.)  We need to finish the job here. */
  if (delete_p)
    {
      XtDestroyWidget(filefed->rowcolumn);

      filefed->rowcolumn = 0;
    }
}

static void
file_free(FEFormData *fed,
	  LO_FormElementData *form)
{
  FEFileFormData *filefed = (FEFileFormData*)fed;

  if (filefed->rowcolumn)
    XtDestroyWidget(filefed->rowcolumn);

  filefed->rowcolumn = 0;
  fed->widget = 0;

  XP_FREE(fed);
}

/*
**
** VTABLE for Text like button elements (BUTTON, RESET, SUBMIT).
**
*/
static void
button_create_widget(FEFormData *fed,
		     LO_FormElementStruct *form)
{
  MWContext *context = fed->context;
  LO_FormElementData *form_data;
  Widget parent;
  int32 form_type;
  unsigned char *string;
  char *name;
  LO_TextAttr *text_attr;
  XmFontList font_list = NULL;
  XmString xmstring = NULL;
  Arg av [50];
  int ac;

  text_attr = XP_GetFormTextAttr(form);
  parent = CONTEXT_DATA (context)->drawing_area;
  form_data = XP_GetFormElementData(form);
  form_type = XP_FormGetType(form_data);
  string = (unsigned char*)XP_FormGetValue(form_data);
  name  = (form_type == FORM_TYPE_SUBMIT
	   ? "formSubmitButton"
	   : (form_type == FORM_TYPE_RESET
	      ? "formResetButton"
	      : "formButton"));

  fe_FontlistAndXmStringForButton(context,
				       form_type,
				       (char*)string,
				       text_attr,
				       &xmstring,
				       &font_list);

#ifdef DEBUG_username
  if (xmstring == NULL) printf ("xmstring is NULL\n");
#endif

  ac = 0;

  if (xmstring)
    {
      XtSetArg (av [ac], XmNlabelString, xmstring); ac++;
    }
  /*#ifdef FORMS_ARE_FONTIFIED*/
  if (font_list)
    {
      XtSetArg (av [ac], XmNfontList, font_list); ac++;
    }
  /*#endif*/
  /* Ok, buttons are always text-colored, because OptionMenu buttons
     must be, and making buttons be a different color looks
     inconsistent.
  */
  if (form_type == FORM_TYPE_SUBMIT)
    {
      XtSetArg (av [ac], XmNshowAsDefault, 1); ac++;
    }
  
  fed->widget = XmCreatePushButton (parent, name, av, ac);

  if (xmstring)
    XmStringFree (xmstring);

/*#ifdef FORMS_ARE_COLORED*/
  set_form_colors(fed, &text_attr->fg, &text_attr->bg);
/*#endif*/
  
#ifndef NO_NEW_DEFAULT_SIZES
  /* added so the security people (or anyone needing an HTML area with
     form elements in it) could specify a wider default width than
     what motif computes based on the font */
  if (form->width > 0
      || form->height > 0)
    {
      Dimension width;
      Dimension height;
      Dimension bw;
      
      XtVaGetValues(fed->widget,
		    XmNheight, &height,
		    XmNwidth, &width,
		    XmNborderWidth, &bw,
		    NULL);
      
      /* take into account our padding */
      width  += fe_FORM_LEFT_MARGIN + fe_FORM_RIGHT_MARGIN;
      height += fe_FORM_TOP_MARGIN + fe_FORM_BOTTOM_MARGIN;
      width  += (bw * 2);
      height += (bw * 2);
      
      if (form->width > width)
	width = form->width;
      if (form->height > height)
	height = form->height;
      
      XtVaSetValues(fed->widget,
		    XmNheight, height,
		    XmNwidth, width,
		    NULL);
    }
#endif
  
  switch (form_type)
    {
    case FORM_TYPE_SUBMIT:
      XtAddCallback (fed->widget,
		     XmNactivateCallback, fe_submit_form_cb, fed);
      break;
    case FORM_TYPE_RESET:
      XtAddCallback (fed->widget,
		     XmNactivateCallback, fe_reset_form_cb, fed);
      break;
    case FORM_TYPE_BUTTON:
      XtAddCallback (fed->widget,
		     XmNactivateCallback, fe_button_form_cb, fed);
      break;
    }
  if (font_list)
     XmFontListFree(font_list);
}

/*
**
** Vtable routines for the radio and checkbox form elements.
**
*/
static void
checkbox_create_widget(FEFormData *fed,
		       LO_FormElementStruct *form)
{
  MWContext *context = fed->context;
  Widget parent = CONTEXT_DATA (context)->drawing_area;
  LO_FormElementData *form_data;
  int32 form_type;
  XmString xmstring;
  int margin = 2; /* leave some slack... */
  int size;
  int descent;
  XmFontList locale_font_list = NULL;
  Arg av[10];
  int ac;
  LO_TextAttr *text_attr;

  form_data = XP_GetFormElementData(form);
  form_type = XP_FormGetType(form_data);

  text_attr = XP_GetFormTextAttr(form);

  fe_font_list_metrics (context, form, locale_font_list, &size, &descent);
  size += (margin * 2);

  XP_FormSetElementToggled(form_data,
			   XP_FormGetDefaultToggled(form_data));
  
  ac = 0;
  xmstring = XmStringCreate (" ", XmFONTLIST_DEFAULT_TAG);
  /* These always use our font list, for proper sizing. */
  XtSetArg (av [ac], XmNfontList, locale_font_list); ac++;
  XtSetArg (av [ac], XmNwidth,  size); ac++;
  XtSetArg (av [ac], XmNheight, size); ac++;
  XtSetArg (av [ac], XmNresize, False); ac++;
  XtSetArg (av [ac], XmNspacing, 2); ac++;
  XtSetArg (av [ac], XmNlabelString, xmstring); ac++;
  XtSetArg (av [ac], XmNvisibleWhenOff, True); ac++;
  XtSetArg (av [ac], XmNset, XP_FormGetDefaultToggled(form_data)); ac++;
  XtSetArg (av [ac], XmNindicatorType,
	    (form_type == FORM_TYPE_RADIO
	     ? XmONE_OF_MANY : XmN_OF_MANY)); ac++;

  fed->widget = XmCreateToggleButton (parent, "toggleForm", av, ac);

  /* These must always be the prevailing color of the text,
     or they look silly. */
#ifdef FORMS_ARE_COLORED
  set_form_colors(fed, &text_attr->fg, &text_attr->bg);
#endif

  if (form_type == FORM_TYPE_RADIO)
    XtAddCallback (fed->widget, XmNvalueChangedCallback,
		   fe_radio_form_cb, fed);
  else
    XtAddCallback (fed->widget, XmNvalueChangedCallback,
		   fe_check_form_cb, fed);

  XmStringFree (xmstring);
}

static void
checkbox_get_value(FEFormData *fed,
		   LO_FormElementStruct *form,
		   XP_Bool delete_p)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  Boolean set = False;

  XtVaGetValues(fed->widget, XmNset, &set, NULL);

  XP_FormSetElementToggled(form_data, set);

  form_element_get_value(fed, form, delete_p);
}

static void
checkbox_change(FEFormData *fed,
		LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  int32 form_type = XP_FormGetType(form_data);
  XP_Bool toggled = XP_FormGetElementToggled(form_data);

  switch (form_type)
    {
    case FORM_TYPE_RADIO:
      if (toggled) LO_FormRadioSet (fed->context, form);
      /* SPECIAL: If this was the only radio button in the radio group,
	 LO_FormRadioSet() will turn it back ON again if Mocha tried to
	 turn it OFF. We want to let mocha be able to turn ON/OFF
	 radio buttons. So we are going to override that and let mocha
	 have its way.
      */
      XFE_SetFormElementToggle(fed->context, form, toggled);
      break;
    case FORM_TYPE_CHECKBOX:
      XtVaSetValues (fed->widget, XmNset, toggled, 0);
      break;
    }
}

/*
**
** Vtable entries for FORM_TYPE_SELECT_MULT
**
*/

static void 
select_create_widget(FEFormData *fed,
			 LO_FormElementStruct *form)
{
  FESelectMultFormData *sel_fed = (FESelectMultFormData*)fed;
  MWContext *context = fed->context;
  Widget parent = CONTEXT_DATA(context)->drawing_area;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  int32 form_type = XP_FormGetType(form_data);
  LO_TextAttr *text_attr;
  int nitems = XP_FormSelectGetOptionsCount(form_data);
  int vlines;
  XmString *items = NULL;
  char *selected_p = NULL;
  int i;
  Arg av[30];
  int ac;
  XmFontList font_list = NULL;

  text_attr = XP_GetFormTextAttr(form);

  vlines = XP_FormSelectGetSize(form_data);

  if (vlines <= 0) vlines = nitems;

  if (nitems > 0) {
    items = (XmString *) malloc (sizeof (XmString) * nitems);
    selected_p = (char *) calloc (nitems, sizeof (char));
  }
  
  for (i = 0; i < nitems; i++)
  {
      lo_FormElementOptionData *d2 =
	XP_FormSelectGetOption(form_data, i);
      selected_p [i] = !!d2->def_selected;
      d2->selected = d2->def_selected;
  }
  fe_FontlistAndXmStringForOptions(context,
					   text_attr,
					   form_data,
					   nitems,
					   items,
					   &font_list);

  ac = 0;
  /* #ifdef FORMS_ARE_FONTIFIED */
  if(NULL != font_list)
  {
    XtSetArg (av [ac], XmNfontList, font_list); ac++;
    if (form_type == FORM_TYPE_SELECT_ONE)
    {
      /* Need to do this untill Ramiro implement fe_ComboBoxSetFontLists */
      XtSetArg (av [ac], XmNlistFontList, font_list); ac++;
    }
  }
  /* #endif */
  XtSetArg (av [ac], XmNspacing, 0); ac++;
  
  if (nitems) {
    XtSetArg (av [ac], XmNitems, items); ac++;
    XtSetArg (av [ac], XmNitemCount, nitems); ac++;
  }

  if (form_type == FORM_TYPE_SELECT_MULT)
    {
      XtSetArg (av [ac], XmNvisibleItemCount, vlines); ac++;
      XtSetArg (av [ac], XmNlistSizePolicy, XmRESIZE_IF_POSSIBLE); ac++;
      XtSetArg (av [ac], XmNselectionPolicy,
		(XP_FormSelectGetMultiple(form_data)
		 ? XmMULTIPLE_SELECT
		 : XmSINGLE_SELECT)); ac++;
      XtSetArg(av[ac], XmNx, -15000); ac++;
    }
  else /* FORM_TYPE_SELECT_ONE */
    {
      Visual *v = 0;
      Colormap cmap = 0;
      Cardinal depth = 0;

      fe_getVisualOfContext (context, &v, &cmap, &depth);

      XtSetArg (av [ac], XmNshadowThickness, 1); ac++;
      XtSetArg (av [ac], XmNarrowType, XmMOTIF); ac++;
      XtSetArg (av [ac], XmNtype, XmDROP_DOWN_LIST_BOX); ac++;
      XtSetArg (av [ac], XmNvisual, v); ac++;
      XtSetArg (av [ac], XmNcolormap, cmap); ac++;
      XtSetArg (av [ac], XmNdepth, depth); ac++;
    }

  if (form_type == FORM_TYPE_SELECT_MULT)
    {
      sel_fed->list_widget = XmCreateScrolledList (parent, "list", av, ac);
      fed->widget = XtParent(sel_fed->list_widget);
      /*
       * We need this Unmanage because otherwise XtIsManaged on the
       * parent fails later.  It seems XmCreateScrolledList() creates
       * an unmanaged list with a managed scrolled window parent.
       * How stupid.
       */
      XtUnmanageChild (fed->widget);
      XtManageChild   (sel_fed->list_widget);
    }
  else /* FORM_TYPE_SELECT_ONE */
    {
      fed->widget = DtCreateComboBox(parent, "list", av, ac);
      XtVaGetValues(fed->widget,
		    XmNlist, &sel_fed->list_widget,
		    NULL);

      /* we also need to set the fontlist explicitly on the label, for
     	some reason... */
      if((NULL != font_list) && 
	 (NULL != fed->widget) 
      ) 
      {

        ac = 0;
        XtSetArg (av [ac], XmNfontList, font_list); ac++;

        /* Still need to do this untill Ramiro implement fe_ComboBoxSetFontLists */
        if(NULL != ((DtComboBoxWidget)(fed->widget))->combo_box.label) 
        	XtSetValues((Widget)((DtComboBoxWidget)(fed->widget))->combo_box.label , av, ac);

      }
    }
    if(NULL != font_list)
    	XmFontListFree(font_list);

#ifdef FORMS_ARE_COLORED
  set_form_colors(fed, &text_attr->fg, &text_attr->bg);
#endif

#ifndef NO_NEW_DEFAULT_SIZES
  /* added so the security people (or anyone needing an HTML area with
     form elements in it) could specify a wider default width (or higher
     default height) than what motif computes based on the font */
  if (form->width > 0)
    {
      Dimension width;
      Dimension height;
      Dimension bw;
      
      XtVaGetValues(sel_fed->list_widget,
		    XmNwidth, &width,
		    XmNheight, &height,
		    XmNborderWidth, &bw,
		    NULL);
      
      /* take into account our padding */
      width  += fe_FORM_LEFT_MARGIN + fe_FORM_RIGHT_MARGIN;
      height += fe_FORM_TOP_MARGIN + fe_FORM_BOTTOM_MARGIN;
      width  += (bw * 2);
      height += (bw * 2);
      
      if (form->width > width)
	width = form->width;
      
      if (form->height > height)
	height = form->height;
      
      XtVaSetValues(sel_fed->list_widget,
		    XmNwidth, width,
		    XmNheight, height,
		    NULL);
    }
#endif
  
  for (i = 0; i < nitems; i++)
    if (selected_p[i])
      if (form_type == FORM_TYPE_SELECT_MULT)
	XmListSelectPos(sel_fed->list_widget, i+1, False);
      else /* FORM_TYPE_SELECT_ONE */
	DtComboBoxSelectItem(fed->widget, items[i]);

  if (form_type == FORM_TYPE_SELECT_MULT)
    {
      XtAddCallback (sel_fed->list_widget,
		     XmNdefaultActionCallback, fe_list_form_cb, fed);
      XtAddCallback (sel_fed->list_widget,
		     XmNsingleSelectionCallback, fe_list_form_cb, fed);
      XtAddCallback (sel_fed->list_widget,
		     XmNmultipleSelectionCallback, fe_list_form_cb, fed);
      XtAddCallback (sel_fed->list_widget,
		     XmNextendedSelectionCallback, fe_list_form_cb, fed);
      XtInsertEventHandler(sel_fed->list_widget,
			   KeyPressMask, False,
			   fe_list_find_eh, context, XtListHead);
      XtAddEventHandler(sel_fed->list_widget,
			FocusChangeMask, FALSE, 
			(XtEventHandler)fe_mocha_focus_notify_eh, 
			fed);
    }
  else
    {
      XtAddCallback(fed->widget,
		    XmNselectionCallback, fe_combo_form_cb, sel_fed);
    }
  
  sel_fed->nkids = nitems;
  sel_fed->selected_p = selected_p;
}

static void 
select_get_value(FEFormData *fed,
		     LO_FormElementStruct *form,
		     XP_Bool delete_p)
{
  FESelectMultFormData *sel_fed = (FESelectMultFormData*)fed;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  int i;
  
  for (i = 0; i < sel_fed->nkids; i ++)
    {
      lo_FormElementOptionData *option_data =
	XP_FormSelectGetOption(form_data, i);
      
      XP_FormOptionSetSelected(option_data,
			       sel_fed->selected_p[i]);
    }

  form_element_get_value(fed, form, delete_p);
}

static void 
select_free(FEFormData *fed,
		LO_FormElementData *form_data)
{
  FESelectMultFormData *sel_fed = (FESelectMultFormData*)fed;

  if (sel_fed->selected_p)
    free(sel_fed->selected_p);

  form_element_free(fed, form_data);
}

static void 
select_reset(FEFormData *fed,
		 LO_FormElementStruct *form)
{
  FESelectMultFormData *sel_fed = (FESelectMultFormData*)fed;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  int32 form_type = XP_FormGetType(form_data);
  int nitems = XP_FormSelectGetOptionsCount(form_data);
  int i;
  XmString *items;

  XtVaGetValues(sel_fed->list_widget, XmNitems, &items, NULL);

  for (i = 0; i < nitems; i++)
    {
      lo_FormElementOptionData *option = XP_FormSelectGetOption(form_data, i);
      XP_Bool selected;

      XP_FormOptionSetSelected(option, XP_FormOptionGetDefaultSelected(option));

      selected = XP_FormOptionGetSelected(option);
      if (selected) {
	/* Highlight the item at pos i+1 if it is not already */
	if (!sel_fed->selected_p[i])
	  if (form_type == FORM_TYPE_SELECT_MULT)
	    XmListSelectPos(sel_fed->list_widget, i+1, False);
	  else /* FORM_TYPE_SELECT_ONE */
	    DtComboBoxSelectItem(fed->widget, items[i]);
      }
      else
	XmListDeselectPos(sel_fed->list_widget, i+1);
      sel_fed->selected_p [i] = selected;
    }
}

static void 
select_change(FEFormData *fed,
		  LO_FormElementStruct *form)
{
  FESelectMultFormData *sel_fed = (FESelectMultFormData*)fed;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  int32 form_type = XP_FormGetType(form_data);
  fe_Font fe_font;
  LO_TextAttr *text_attr;
  XmString *new_items;
  char* new_selected_p;
  int nitems;
  int i;
  XmFontList fontlist = NULL;
  XP_Bool item_selected = FALSE;
  XmFontType type;
  XP_Bool use_UnicodePseudoFont;
  MWContext *context = fed->context;

  nitems = XP_FormSelectGetOptionsCount(form_data);
  text_attr = XP_GetFormTextAttr(form);
  use_UnicodePseudoFont = IS_UNICODE_CSID(text_attr->charset);
  
  if(use_UnicodePseudoFont)
  {
      Widget parent = CONTEXT_DATA(context)->drawing_area;
      fe_font = fe_LoadFontForWidgetUnicodePseudoFont(context, parent, text_attr, &fontlist, &type,
         				text_attr->fontmask);
      if(fontlist)
         fontlist = XmFontListCopy(fontlist);
  }
  else
  {
      fe_font = fe_LoadFontFromFace(
                            context, text_attr, 
                            &text_attr->charset,
                            text_attr->font_face, 
                            text_attr->size,            
                            text_attr->fontmask          
      );
      FE_NONEDIT_FONTLIST(text_attr->charset, fe_font, fontlist);
  }
  
  if (nitems > 0)
    {
      new_items = (XmString *) malloc (sizeof (XmString) * nitems);
      new_selected_p = (char *) calloc (nitems, sizeof (char));
    }

#ifdef DEBUG_username
  printf ("nitems = %d\n", nitems);
#endif

  for (i = 0; i < nitems; i ++)
    {
      char *str;
      lo_FormElementOptionData *option_data =
	XP_FormSelectGetOption(form_data, i);

      str = (option_data->text_value 
	     ? ((*((char *) option_data->text_value)) 
		? ((char *) option_data->text_value) 
		: " ") 
	     : "---\?\?\?---");

      if(use_UnicodePseudoFont)
        {
             new_items[i] = fe_ConvertToXmString(
				     str,
				     text_attr->charset,
				     fe_font,
				     type,
				     &fontlist);
        }
      else 
        {
             new_items[i] = FE_NONEDIT_TO_XMSTRING(text_attr->charset,
                                           fe_font,
                                           str,
                                           XP_STRLEN(str)
                                           );
        }
      new_selected_p [i] = !!XP_FormOptionGetSelected(option_data);

#ifdef DEBUG_username
      printf ("items[%d] = %s\n", i, str);
#endif
    }
  
  XmListDeselectAllItems(sel_fed->list_widget);

  XtVaSetValues(sel_fed->list_widget,
		XmNitems, new_items,
		XmNitemCount, nitems,
		NULL);

  for (i = 0; i < nitems; i++)
    if (new_selected_p[i])
      if (form_type == FORM_TYPE_SELECT_MULT)
	XmListSelectPos(sel_fed->list_widget, i+1, False);
      else /* FORM_TYPE_SELECT_ONE */
	{
	  DtComboBoxSelectItem(fed->widget, new_items[i]);
	  item_selected = TRUE;
	}

  if (sel_fed->selected_p)
    free(sel_fed->selected_p);

  sel_fed->selected_p = new_selected_p;
  sel_fed->nkids = nitems;
}

/*
**
** Vtable entries for FORM_TYPE_TEXTAREA
**
*/

static void
textarea_create_widget(FEFormData *fed,
		       LO_FormElementStruct *form)
{
  FETextAreaFormData *ta_fed = (FETextAreaFormData*)fed;
  MWContext *context = fed->context;
  Widget parent;
  int16 charset;
  LO_FormElementData *form_data;
  int32 form_type;
  int32 rows, cols;
  char *text;
  unsigned char *loc;
  XmFontList locale_font_list;
  Arg av [50];
  int ac;
  LO_TextAttr *text_attr;

  parent = CONTEXT_DATA (context)->drawing_area;
  form_data = XP_GetFormElementData(form);
  form_type = XP_FormGetType(form_data);
  text_attr = XP_GetFormTextAttr(form);
  text = (char*)XP_FormGetDefaultText(form_data);
  charset = text_attr->charset;

  XP_FormTextAreaGetDimensions(form_data,
			       &rows, &cols);

  
  /* the default size for text fields is 20 columns. 
     XX Shouldn't the backend just fill it in with 20? */
  if (cols == 0) cols = 20;

  if (!text) text = "";

  fe_forms_clean_text (context, charset, text, False);
  loc = fe_ConvertToLocaleEncoding (charset, text);

  fe_FontlistAndXmStringForFormElement(context,
				       form_type,
				       loc,
				       text_attr,
				       NULL,
				       &locale_font_list);
  if (!locale_font_list)
	return;
  
  ac = 0;
  
  XtSetArg (av[ac], XmNscrollingPolicy, XmAUTOMATIC); ac++;
  XtSetArg (av[ac], XmNvisualPolicy, XmCONSTANT); ac++;
  XtSetArg (av[ac], XmNscrollBarDisplayPolicy, XmSTATIC); ac++;
  
  XtSetArg (av[ac], XmNvalue, loc); ac++;
  XtSetArg (av[ac], XmNcursorPosition, 0); ac++;
  /* from bstell: in multibyte (asian) locales, characters tend to be twice
     as wide as in non-multibyte locales.  To make things look correct,
     we divide the column number in half in those cases. */
  XtSetArg (av[ac], XmNcolumns, ((fe_LocaleCharSetID & MULTIBYTE) ?
				 (cols + 1) / 2 : cols)); ac++;
  XtSetArg (av[ac], XmNrows, rows); ac++;
  /*
   * Gotta love Motif.  No matter what wordWrap is set to, if
   * there is a horizontal scrollbar present it won't wrap.
   * Of course the documentation mentions this nowhere.
   * "Use the source Luke!"
   */
  if (XP_FormTextAreaGetAutowrap(form_data) != TEXTAREA_WRAP_OFF)
    {
      XtSetArg (av[ac], XmNscrollHorizontal, FALSE); ac++;
      XtSetArg (av[ac], XmNwordWrap, TRUE); ac++;
    }
  XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
  /* Ok, let's try using the fixed font for text areas only. */
  /*#ifdef FORMS_ARE_FONTIFIED*/
  XtSetArg (av[ac], XmNfontList, locale_font_list); ac++;
  /*#endif*/
  XtSetArg(av[ac], XmNx, -15000); ac++;
  ta_fed->text_widget = XmCreateScrolledText (parent, "formText", av, ac);
  ta_fed->form_data.widget = XtParent(ta_fed->text_widget);

  /* The scroller must have the prevailing color of the text,
     or it looks funny.*/
#ifdef FORMS_ARE_COLORED
  set_form_colors(fed, &text_attr->fg, &text_attr->bg);
#endif

  if (((char *) loc) != text)
    {
      XP_FREE (loc);
    }

  XtAddCallback(ta_fed->text_widget, XmNlosingFocusCallback,
		fe_lost_focus_cb, fed);
  XtAddCallback(ta_fed->text_widget, XmNfocusCallback, fe_got_focus_cb, fed);
  XtAddEventHandler(ta_fed->text_widget,
		    KeyPressMask | KeyReleaseMask,
		    FALSE, /* don't care about nonmaskable events */
		    fe_key_handler,
		    ta_fed);

  /* Add verify callbacks to implement JS key events */
  fe_HackTextTranslations_addCallbacks(ta_fed->text_widget);

  XtUnmanageChild (ta_fed->form_data.widget);
  XtManageChild (ta_fed->text_widget);
}

static void
textarea_display(FEFormData *fed,
		 LO_FormElementStruct *form)
{
  form_element_display(fed, form);
}

static void
textarea_get_value(FEFormData *fed,
		   LO_FormElementStruct *form,
		   XP_Bool delete_p)
{
  MWContext *context = fed->context;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FETextAreaFormData *ta_fed = (FETextAreaFormData*)fed;
  INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(context);
  char *text = 0;
  int32 cols;
  PA_Block current_text, new_current_text;
  PA_Block default_text;

  XP_FormTextAreaGetDimensions(form_data, NULL, &cols);

  XtVaGetValues (ta_fed->text_widget, XmNvalue, &text, 0);
  if (! text) return;
  if (XP_FormTextAreaGetAutowrap(form_data) == TEXTAREA_WRAP_HARD) {
    char *tmp = XP_WordWrap(fe_LocaleCharSetID, text, cols, 0);
    if (text) XtFree(text);
    if (!tmp) return;
    text = tmp;
  }

  current_text = XP_FormGetCurrentText(form_data);
  default_text = XP_FormGetDefaultText(form_data);

  if (current_text && current_text != default_text)
    free (current_text);

  new_current_text = 
    (PA_Block)fe_ConvertFromLocaleEncoding (INTL_GetCSIWinCSID(c),
					    (unsigned char *) text);

  XP_FormSetCurrentText(form_data, new_current_text);

  if (((char *) new_current_text) != text) {
    free (text);
  }

  form_element_get_value(fed, form, delete_p);
}

static void
textarea_reset(FEFormData *fed,
	       LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FETextAreaFormData *ta_fed = (FETextAreaFormData*)fed;
  char *default_text = (char*)XP_FormGetDefaultText(form_data);
  LO_TextAttr *text_attr = XP_GetFormTextAttr(form);
  int16 charset = text_attr->charset;
  unsigned char *loc;

  if (!default_text) default_text = "";

  fe_forms_clean_text (fed->context, charset, default_text, False);

  XtVaSetValues (ta_fed->text_widget, XmNcursorPosition, 0, 0);

  loc = fe_ConvertToLocaleEncoding (charset, (unsigned char*)default_text);

  XtVaSetValues (ta_fed->text_widget, XmNvalue, loc, 0);

  if (((char *) loc) != default_text)
    {
      XP_FREE (loc);
    }
}

static void
textarea_lost_focus(FEFormData *fed)
{
  FETextAreaFormData *ta_fed = (FETextAreaFormData*)fed;
  LO_FormElementData *form_data;
  char *text;
  XP_Bool text_changed = False;
  PA_Block current_text;
  JSEvent *event;

  form_data = XP_GetFormElementData(fed->form);

  current_text = XP_FormGetCurrentText(form_data);

  XtVaGetValues (ta_fed->text_widget, XmNvalue, &text, 0);

  if (!current_text || XP_STRCMP((char*)current_text, text))
	text_changed = True;

  if (((char*) current_text) != text) {
	XtFree (text);
  }

  /* if the text has changed, call get_element_value to copy it into the form
     element, and send a CHANGE event to the javascript thread. */
  if (text_changed)
	{
		  (*fed->vtbl.get_element_value)(fed, fed->form, FALSE);

		  event = XP_NEW_ZAP(JSEvent);

		  event->type = EVENT_CHANGE;

		  ET_SendEvent (fed->context, (LO_Element *) fed->form, event,
						NULL, NULL);
	  }
}

static FEFormData *
alloc_form_data(int32 form_type)
{
  FEFormData *data;

  switch (form_type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_READONLY:
	  data = (FEFormData*)XP_NEW_ZAP(FEFormData);
	  data->vtbl = text_form_vtable;
	  return data;

	case FORM_TYPE_FILE:
	  data = (FEFormData*)XP_NEW_ZAP(FEFileFormData);
	  data->vtbl = file_form_vtable;
	  return data;

	case FORM_TYPE_SUBMIT:
	case FORM_TYPE_RESET:
	case FORM_TYPE_BUTTON:
	  data = (FEFormData*)XP_NEW_ZAP(FEFormData);
	  data->vtbl = button_form_vtable;
	  return data;

	case FORM_TYPE_RADIO:
	case FORM_TYPE_CHECKBOX:
	  data = (FEFormData*)XP_NEW_ZAP(FEFormData);
	  data->vtbl = checkbox_form_vtable;
	  return data;

	case FORM_TYPE_SELECT_ONE:
	  data = (FEFormData*)XP_NEW_ZAP(FESelectOneFormData);
	  data->vtbl = selectone_form_vtable;
	  return data;

	case FORM_TYPE_TEXTAREA:
	  data = (FEFormData*)XP_NEW_ZAP(FETextAreaFormData);
	  data->vtbl = textarea_form_vtable;
	  return data;

	case FORM_TYPE_SELECT_MULT:
	  data = (FEFormData*)XP_NEW_ZAP(FESelectMultFormData);
	  data->vtbl = selectmult_form_vtable;
	  return data;

	case FORM_TYPE_HIDDEN:
	case FORM_TYPE_JOT:
	case FORM_TYPE_ISINDEX:
	case FORM_TYPE_IMAGE:
	case FORM_TYPE_KEYGEN:
	case FORM_TYPE_OBJECT:
	default:
	  XP_ASSERT(0);
	  return NULL;
	}
}

/*
** This method can be called more than once for the same front end form element.
** 
** In those cases we don't reallocate the FEData*, and also don't recreate the widget,
** but we do set the form and context pointers in FEData, and we call the get_size
** method.
**
** The two cases I know of where layout calls this function more than once are:
** when laying out a table with form elements in it -- layout moves our fe data
** over to the new LO_FormElementStruct, but we have to sync up our pointer to
** the new.
** when reloading a page with frames in it.  In this case, the context is different,
** so need to always set that as well.
**
** Also, layout could have told us to delete the widget without freeing our FEData*
** (this happens with delete_p is TRUE in XFE_GetFormElementValue.)  In this case,
** we need to recreate the widget.
**
** yuck.
*/
void
XFE_GetFormElementInfo(MWContext *context,
		       LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;

#ifdef DEBUG_username
  printf ("XFE_GetFormElementInfo\n");
#endif

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);
  if (!fed)
	{
	  int32 form_type = XP_FormGetType(form_data);

	  fed = alloc_form_data(form_type);

	  XP_ASSERT(fed);
	  if (!fed) return;

	  XP_FormSetFEData(form_data, fed);
	}

  fed->form = form;
  fed->context = context;

  if (!fed->widget)
    {
      XP_ASSERT(fed->vtbl.create_widget_func);

      (*fed->vtbl.create_widget_func)(fed, form);
    }
      
  XP_ASSERT(fed->vtbl.get_size_func);
  
  (*fed->vtbl.get_size_func)(fed, form);
}

void
XFE_DisplayFormElement(MWContext *context, int iLocation,
					   LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;

#ifdef DEBUG_username
  printf ("XFE_DisplayFormElement\n");
#endif

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);

  XP_ASSERT(fed);
  if (!fed) return;

  if (fed->vtbl.display_element_func)
    (*fed->vtbl.display_element_func)(fed, form);
}

void
XFE_FormTextIsSubmit (MWContext *context, LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);

  XP_ASSERT(fed);
  if (!fed) return;

  if (fed->vtbl.element_is_submit_func)
    (*fed->vtbl.element_is_submit_func)(fed, form);
}

void
XFE_GetFormElementValue (MWContext *context, LO_FormElementStruct *form,
						 XP_Bool delete_p)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;

#ifdef DEBUG_username
  printf ("XFE_GetFormElementValue (delete_p = %d)\n", delete_p);
#endif

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);

  XP_ASSERT(fed);
  if (!fed) return;

  if (fed->vtbl.get_element_value)
    (*fed->vtbl.get_element_value)(fed, form, delete_p);
}

void
FE_FreeFormElement(MWContext *context,
		   LO_FormElementData *form_data)
{
  FEFormData *fed;

#ifdef DEBUG_username
  printf ("XFE_FreeFormElement. \n");
#endif

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);

  /* this assert gets tripped by HIDDEN form elements, since we have no
     FE data associated with them. */
  /*XP_ASSERT(fed);*/
  if (!fed) return;

  if (fed->vtbl.free_element_func)
    (*fed->vtbl.free_element_func)(fed, form_data);

  /* clear out the FE data, so we don't try anything funny from now on. */
  XP_FormSetFEData(form_data, NULL);
}

void
FE_FocusInputElement(MWContext *context,
		     LO_Element *element)
{
  if (element)
    {
      LO_FormElementData *form_data =
	XP_GetFormElementData((LO_FormElementStruct*)element);
      FEFormData* fed;

      if (!form_data)
	return;
      
      fed = (FEFormData*)XP_FormGetFEData(form_data);

      if (fed->vtbl.focus_element_func)
	(*fed->vtbl.focus_element_func)(fed, (LO_FormElementStruct*)element);
    }
  else
    {
      JSEvent *event;
      
      if (context->is_grid_cell)
	fe_SetGridFocus (context);
      XmProcessTraversal (CONTEXT_WIDGET(context), XmTRAVERSE_CURRENT);
      FE_RaiseWindow(context);
      
      event = XP_NEW_ZAP(JSEvent);
      event->type = EVENT_FOCUS;
      ET_SendEvent (context, element, event, NULL, NULL);
    }
}

/*
 * Force input to be defocused from an element in the given window.
 * It's ok if the input didn't have input focus.
 */
void
FE_BlurInputElement(MWContext *context, LO_Element *element)
{
  /* Just blow away focus */    
  TRACEMSG(("XFE_BlurInputElement: element->type = %d\n"));

  if (!context) return;

  /* I was told by brendan that this is unnecessary - ramiro */
#if 0
  if (form == NULL || form->element_data == NULL) {
#ifdef DEBUG_spence    
    printf ("BlurInputElement: form is invalid\n");
#endif
    return;
  }
#endif

  fe_NeutralizeFocus (context);

  if (!element) {
	  FE_LowerWindow(context);

	  fe_MochaBlurNotify(context, element);
  }
}

void
FE_SelectInputElement(MWContext *context, LO_Element *element)
{
  LO_FormElementStruct *form = (LO_FormElementStruct*)element;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);

  XP_ASSERT(fed);
  if (!fed) return;

  if (fed->vtbl.select_element_func)
      (*fed->vtbl.select_element_func)(fed, form);
}

void
FE_ChangeInputElement(MWContext *context,
		      LO_Element *element)
{
  LO_FormElementStruct *form = (LO_FormElementStruct*)element;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;
  int32 type;

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);
  type = XP_FormGetType(form_data);

  /* this assert gets tripped by HIDDEN form elements, since we have no
     FE data associated with them. */
  /*XP_ASSERT(fed);*/
  if (!fed) return;

  if (fed->vtbl.change_element_func)
    {
      LO_LockLayout();

      (*fed->vtbl.change_element_func)(fed, form);
      
      LO_UnlockLayout();
    }
}

/*
** Tell the FE that a form is being submitted without a UI gesture indicating
** that fact, i.e., in a Mocha-automated fashion ("document.myform.submit()").
** The FE is responsible for emulating whatever happens when the user hits the
** submit button, or auto-submits by typing Enter in a single-field form.
*/
void
FE_SubmitInputElement(MWContext *context, LO_Element *element)
{
  LO_FormSubmitData *submit;
  URL_Struct        *url;
  History_entry *he = NULL;

  TRACEMSG(("XFE_SubmitInputElement: element->type = %d\n"));
  if (!context) return;

  if (element == NULL) {
#ifdef DEBUG_username    
    printf ("SubmitInputElement: form is invalid\n");
#endif
    return;
  }

  submit = LO_SubmitForm (context, (LO_FormElementStruct *) element);
  if (!submit)
    return;

  /* Create the URL to load */
  url = NET_CreateURLStruct((char *)submit->action, NET_DONT_RELOAD);

#if defined(SingleSignon)
  /* Check for a password submission and remember the data if so */
  SI_RememberSignonData(context, submit);
#endif

  /* ... add the form data */
  NET_AddLOSubmitDataToURLStruct(submit, url);

  /* referrer field if there is one */
  he = SHIST_GetCurrent (&context->hist);
  url->referer = fe_GetURLForReferral(he);

  fe_GetURL (context, url, FALSE);
  LO_FreeSubmitData (submit);
}

/*
 * Emulate a button or HREF anchor click for element.
 */
void
FE_ClickInputElement(MWContext *context, LO_Element *xref)
{
  LO_FormElementStruct *form = (LO_FormElementStruct *) xref;
  LO_FormElementData *form_data;
  FEFormData *fed;

  if (!form)
    return;

  form_data = XP_GetFormElementData(form);
  if (!form_data)
    return;

  fed = XP_FormGetFEData(form_data);

  switch (xref->type) {
  case LO_FORM_ELE:
    {
      XmPushButtonCallbackStruct cb;
      cb.reason = XmCR_ACTIVATE;
      cb.event = 0;
      cb.click_count = 1;
      XtCallCallbacks (fed->widget, XmNactivateCallback, &cb);
    }
    break;
  case LO_IMAGE:
  case LO_TEXT:
    {
      CL_Event layer_event;
      fe_EventStruct fe_event;
      XEvent event;

      event.type = ButtonPress;
      event.xbutton.time = XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context)));
      /* We only fill in the fields that we need. */
#ifdef LAYERS_FULL_FE_EVENT
      fe_event.event = &event;
#else
      fe_event_stuff(context,&fe_event,&event,0,0,FE_INVALID_MOUSE_ACTION);
      layer_event.fe_event_size = sizeof(fe_event);
#endif
      layer_event.fe_event = (void*)&fe_event;
      layer_event.x = xref->lo_image.x;
      layer_event.y = xref->lo_image.y; 
      (void) fe_HandleHREF (context, xref, False, False, &layer_event, 0);
    }
    break;
  }
}

static void
fe_form_file_browse_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  FEFormData *fed = (FEFormData*) closure;
  char *filename;
  XmString xm_title = 0;
  char *title = 0;

  XtVaGetValues (widget, XmNlabelString, &xm_title, 0);
  if (xm_title)
    XmStringGetLtoR (xm_title,
		     XmFONTLIST_DEFAULT_TAG, &title); /* title - XtMalloc*/
  XmStringFree(xm_title);

  filename = fe_ReadFileName( fed->context, title, 0, FALSE, 0);
  XtFree(title);

  if (filename) {
    XP_StatStruct st;
    char buf [2048];
    if (stat(filename, &st) < 0) 
      {
	/* Error: Cant stat */
	PR_snprintf (buf, sizeof (buf),
		     XP_GetString(XFE_ERROR_OPENING_FILE), filename);
	FE_Alert(fed->context, buf);
      }
    else 
      {
	/* The file was found. Stick the name into the text field. */
	XmTextFieldSetString (fed->widget, filename);
	
	/* resync up the backend with the current value in the text field. */
	XFE_GetFormElementValue(fed->context, fed->form, False);
      }

    XP_FREE(filename);
  }
}

/* This happens for self submiting forms. */
static void
fe_activate_submit_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  FEFormData *fed = (FEFormData*)closure;

  /* Move focus out of the current focus widget. This will make any OnChange
   * calls to mocha.
   */
  fe_NeutralizeFocus(fed->context);

  fe_submit_form_cb(widget, closure, call_data);
}

static void
fe_mocha_focus_notify_eh (Widget w, 
			  XtPointer closure, 
			  XEvent *ev, 
			  Boolean *cont)
{
  FEFormData *fed = (FEFormData *) closure;
  MWContext *context = (MWContext *) fed->context;

  TRACEMSG (("fe_focus_notify_eh\n"));
  switch (ev->type) {
  case FocusIn:
    TRACEMSG (("focus in\n"));
    fe_MochaFocusNotify(context, (LO_Element*)fed->form);
    break;
  case FocusOut:
    TRACEMSG (("focus out\n"));
    fe_MochaBlurNotify(context, (LO_Element*)fed->form);
    break;
  }
}

static void
fe_mocha_submit_form_cb (MWContext *context, LO_Element *element, int32 event,
			 void *closure, ETEventStatus status)
{
  LO_FormSubmitData *data;
  URL_Struct *url;
  
  if (status != EVENT_OK)
    return;
  
  data = LO_SubmitForm (context, (LO_FormElementStruct *) element);
  if (! data) return;
  url = NET_CreateURLStruct ((char *) data->action, FALSE);
  
  /* Add the referer to the URL. */
  {
    History_entry *he = SHIST_GetCurrent (&context->hist);
    if (url->referer)
      free (url->referer);
    url->referer = fe_GetURLForReferral(he);
  }

  NET_AddLOSubmitDataToURLStruct (data, url);
  if (data->window_target)
  {
	  MWContext *tempContext;

      tempContext = XP_FindNamedContextInList(context, data->window_target);

	  /* Might be null if we're looking at _new. */
	  if(tempContext)
		  {
			  context = tempContext;
			  data->window_target = NULL;
			  url->window_target = NULL;
		  }
  }


  fe_GetURL (context, url, FALSE);
  LO_FreeSubmitData (data);
}


static void
fe_mocha_submit_click_form_cb(MWContext *context, LO_Element *element,
			      int32 event, void *closure,
			      ETEventStatus status)
{
  FEFormData *fed = (FEFormData *) closure;

  if (status != EVENT_OK)
	return;

  /* Load the element value before telling mocha about the submit.
   * This needs to be done if we came here not because of the user
   * clicking on the submit button, like hitting RETURN on a text field.
   */
  XFE_GetFormElementValue(fed->context, fed->form, False);
  
  {
	JSEvent *event = XP_NEW_ZAP(JSEvent);

	event->type = EVENT_SUBMIT;

	ET_SendEvent (fed->context, (LO_Element *) fed->form, event,
				  fe_mocha_submit_form_cb, NULL);
  }
}

static void
fe_submit_form_cb (Widget widget, XtPointer closure, XtPointer call_data)
/*
 * description:
 *	This function was registered as an XmNactivateCallback for
 *	a Motif PushButton widget created in XFE_GetFormElementInfo().  
 *
 * preconditions:
 *	widget != 0 && closure != 0 && call_data != 0
 *
 * returns:
 *	not applicable
 *
 **********************************************************************/
{
  FEFormData *fed = (FEFormData *) closure;
  /*LO_FormSubmitData *data;*/
  /*URL_Struct *url;*/

#ifdef DEBUG
  /* check preconditions in debug mode */
  if (widget == 0 || closure == 0 || call_data == 0) {
      fprintf(stderr, private_precondition_format, __FILE__, __LINE__);
      fflush(stderr);
    }
#endif


  {
    XEvent  *x_event;
    JSEvent *js_event;
    XP_Bool do_geometry = FALSE;

    /*
     * The Motif documentation for XmPushButton informs that call_data
     * is a pointer to XmPushButtonCallbackStruct, declared in Xm/Xm.h.
     */
    x_event = ((XmPushButtonCallbackStruct*)call_data)->event;
    js_event = XP_NEW_ZAP(JSEvent);

    js_event->type = EVENT_CLICK;
    fe_complete_js_event(js_event, x_event, do_geometry);

    /* Lord Whorfin says: send me a click, dammit! */
    ET_SendEvent (fed->context, (LO_Element *) fed->form, js_event,
		  fe_mocha_submit_click_form_cb, closure);
  }
}

static void
fe_mocha_button_form_cb (MWContext *context, LO_Element *element, int32 event,
			void *closure, ETEventStatus status)
{
  if (status != EVENT_OK)
    return;

  XFE_GetFormElementValue (context, (LO_FormElementStruct *) element, False);
}



/**********************************************************************
 */

static void
fe_button_form_cb (Widget widget,       /* in: a non-NULL valid widget */
		   XtPointer closure,   /* in: registered by us in XtAddCallback() */
		   XtPointer call_data) /* in: from Xt */

/*
 * description:
 *	Called when a form button is clicked.
 *	This function was registered as an XmNactivateCallback for
 *	a Motif PushButton widget created in XFE_GetFormElementInfo().
 *
 * preconditions:
 *	widget != 0 && closure != 0 && call_data != 0
 *
 * returns:
 *	not applicable
 *
 **********************************************************************/
{
  FEFormData *form_data = (FEFormData *) closure;
  XEvent  *x_event;  /* X Windows event structure */
  JSEvent *js_event; /* Javascript event */
  XP_Bool do_geometry = FALSE;

#ifdef DEBUG
  /* check preconditions in debug mode */
  if (widget == 0 || closure == 0 || call_data == 0) {
      fprintf(stderr, private_precondition_format, __FILE__, __LINE__);
      fflush(stderr);
    }
#endif
  /*
   * The Motif documentation for XmPushButton informs that call_data
   * is a pointer to XmPushButtonCallbackStruct, declared in Xm/Xm.h.
   */
  x_event = ((XmPushButtonCallbackStruct*)call_data)->event;
  js_event = XP_NEW_ZAP(JSEvent);

  js_event->type = EVENT_CLICK;
  fe_complete_js_event(js_event, x_event, do_geometry);
  ET_SendEvent (form_data->context,
		(LO_Element *) form_data->form,
		js_event,
		fe_mocha_button_form_cb,
		NULL);
}

static void
fe_mocha_reset_form_cb (MWContext *context, LO_Element *element, int32 event,
			void *closure, ETEventStatus status)
{
  if (status != EVENT_OK)
    return;

  LO_ResetForm (context, (LO_FormElementStruct *) element);
}

/**********************************************************************
 */

static void
fe_reset_form_cb (Widget widget, XtPointer closure, XtPointer call_data)

/*
 * description:
 *	This function was registered as an XmNactivateCallback for
 *	a Motif PushButton widget created in XFE_GetFormElementInfo().  
 *
 * preconditions:
 *	widget != 0 && closure != 0 && call_data != 0
 *
 * returns:
 *	not applicable
 *
 **********************************************************************/
{
  FEFormData *fed = (FEFormData *) closure;

#ifdef DEBUG
  /* check preconditions in debug mode */
  if (widget == 0 || closure == 0 || call_data == 0) {
      fprintf(stderr, private_precondition_format, __FILE__, __LINE__);
      fflush(stderr);
    }
#endif

  {
    XEvent  *x_event;
    JSEvent *js_event;
    XP_Bool do_geometry = FALSE;

    /*
     * The Motif documentation for XmPushButton informs that call_data
     * is a pointer to XmPushButtonCallbackStruct, declared in Xm/Xm.h.
     */
    x_event = ((XmPushButtonCallbackStruct*)call_data)->event;
    js_event = XP_NEW_ZAP(JSEvent);

    js_event->type = EVENT_CLICK;
    fe_complete_js_event(js_event, x_event, do_geometry);

    ET_SendEvent (fed->context, (LO_Element *) fed->form, js_event,
		  fe_mocha_reset_form_cb, NULL);
  }
}


static void
fe_got_focus_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  FEFormData *fed = (FEFormData *) closure;
  JSEvent *event;

  TRACEMSG(("fe_got_focus_c:\n"));
  
  event = XP_NEW_ZAP(JSEvent);
  event->type = EVENT_FOCUS;
  
  ET_SendEvent (fed->context, (LO_Element *) fed->form, event, NULL, NULL);
}

static void
fe_lost_focus_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  FEFormData *fed = (FEFormData *) closure;

  if (!fed) return;

  if (fed->vtbl.lost_focus_func)
    (*fed->vtbl.lost_focus_func)(fed);

  /* always send a blur event. */
  fe_MochaBlurNotify(fed->context, (LO_Element*)fed->form);
}

static void
fe_complete_js_event(JSEvent *js_event,    /* inout */
		   XEvent  *x_event,     /* in */
		   XP_Bool  do_geometry) /* in */

/*
 * description:
 *	Updates js_event fields from an X event.
 *	Consolidates common code inside various callback
 *	functions within this module.
 *
 * preconditions:
 *	js_event != 0 && x_event != 0
 *	The js_event must already have an assigned type.
 *
 * returns:
 *	not applicable
 *
 ****************************************/
{
  unsigned int state_of_buttons_and_modifiers = 0;

#ifdef DEBUG
  /* check preconditions in debug mode */
  if (js_event == 0 || x_event == 0 || js_event->type == 0) {
    fprintf(stderr, private_precondition_format, __FILE__, __LINE__);
    fflush(stderr);
  }
#endif

  state_of_buttons_and_modifiers  = x_event->xbutton.state;

  switch(js_event->type) {
  case EVENT_CLICK:
#ifdef DEBUG_rodt
    /* check in debug mode */
    /* Expect to be processing ButtonRelease */
    if (x_event->type != ButtonRelease) {
      fprintf(stderr, private_check_format, __FILE__, __LINE__);
      fflush(stderr);
    }
#endif
    {
      unsigned int button;

      button = x_event->xbutton.button;

      /*
       * MOUSE BUTTON
       *
       * Only verified that left button is 1.  2 should be
       * middle button and 3 should be right button.
       */
      js_event->which = button;

    }
    break;
  case EVENT_KEYDOWN:
  case EVENT_KEYUP:
  case EVENT_KEYPRESS:
#ifdef DEBUG
    /* check in debug mode */
    /* Expect to be processing KeyPress or KeyRelease */
    if (x_event->type != KeyPress && x_event->type != KeyRelease) {
      fprintf(stderr, private_check_format, __FILE__, __LINE__);
      fflush(stderr);
    }
#endif
    {
      int buffer_length;
      char buffer[32];
      KeySym keysym;
      XComposeStatus * compose_status;

      /*
       * DETERMINE ASCII VALUE OF KEY PRESSED FOR JAVASCRIPT
       *
       * The following code needs to be checked on corner cases.
       * 
       */

      /*
       * Ignore compose status.
       * This may need to be changed.
       */
      compose_status = 0;

      /*
       * The XEvent structure is converted to a keysym as well as a
       * string (possibly more than one character) which represents
       * the keysym.
       *
       */
      buffer_length = XLookupString(&(x_event->xkey),
				    buffer,
				    31,
				    &keysym,
				    compose_status);
      if (buffer_length == 1)
	{
	  js_event->which = buffer[0];
	}
      else
	{
	  /*
	   * Force all other cases to return a zero to 
	   * expose bugs which indicate that this code needs
	   * to be modified.
	   */
	  js_event->which = 0;
	}

      if (do_geometry) {
	  js_event->x       = x_event->xkey.x;
	  js_event->y       = x_event->xkey.y;
	  js_event->screenx = x_event->xkey.x_root;
	  js_event->screeny = x_event->xkey.y_root;
      }
    }
  break;
  }

  /*
   * ASSIGN MODIFIERS
   */
  if (state_of_buttons_and_modifiers) {
    /*
     * MODIFIER KEYS (ALT, CONTROL, SHIFT, META)
     * EVENT_xxx_MASK's are from ns/include/libevent.h
     */
    if (state_of_buttons_and_modifiers & ControlMask)
      js_event->modifiers |= EVENT_CONTROL_MASK;
    if (state_of_buttons_and_modifiers & ShiftMask)
      js_event->modifiers |= EVENT_SHIFT_MASK;

    /*
     * Only verified meta on Solaris 2.5 dtwm (mwm workalike)
     */
    if (state_of_buttons_and_modifiers & Mod4Mask)
      js_event->modifiers |= EVENT_META_MASK;
  }
}

static void
fe_key_handler(Widget widget,       /* in */
	       XtPointer closure,   /* in */
	       XEvent * x_event,    /* in */
	       Boolean * bcontinue) /* out */

/*
 * description:
 *	Handles XEvents of type KeyPress and KeyRelease in order
 *	to pass these along to JavaScript.  JavaScript recognizes
 *	three key events:
 *	EVENT_KEYDOWN  corresponds to the X Event KeyPress
 *	EVENT_KEYUP    corresponds to the X Event KeyRelease
 *	EVENT_KEYPRESS corresponds to the X Event KeyPress followed by KeyRelease
 *
 *	Do not confuse EVENT_KEYPRESS with the X Event KeyPress!
 *
 * returns:
 *	Not applicable.  Note that currently bcontinue is not modified.
 *
 * end:
 ****************************************/
{
  FEFormData *form_data;
  JSEvent *js_event = 0; /* Javascript event */
  static int32 previous_js_event_type = 0;
  static int32 previous_js_event_which = 0;


#ifdef DEBUG
  /* check preconditions in debug mode */
  if (widget == 0 || closure == 0 || x_event == 0 || bcontinue == 0) {
      fprintf(stderr, private_precondition_format, __FILE__, __LINE__);
      fflush(stderr);
    }
#endif

  form_data = (FEFormData *) closure;

  if (x_event->type == KeyPress) {
      js_event = XP_NEW_ZAP(JSEvent);
      js_event->type = EVENT_KEYDOWN;
  }
  else if (x_event->type == KeyRelease) {
      js_event = XP_NEW_ZAP(JSEvent);
      js_event->type = EVENT_KEYUP;
  }

  /*
   * Do further work only on KeyPress and KeyRelease
   */
  if (js_event) {
    XP_Bool do_geometry = TRUE;

    fe_complete_js_event(js_event, x_event, do_geometry);

    /*
     * SYNTHESIZE AND SEND EVENT_KEYPRESS
     */
    if (previous_js_event_type == EVENT_KEYDOWN
	&& js_event->type == EVENT_KEYUP
	&& previous_js_event_which == js_event->which)
      {
	JSEvent *synthesized_js_event;
	
	synthesized_js_event = XP_NEW_ZAP(JSEvent);
	XP_MEMCPY(synthesized_js_event, js_event, sizeof(JSEvent));
	synthesized_js_event->type = EVENT_KEYPRESS;
	ET_SendEvent(form_data->context,
		     (LO_Element *) form_data->form,
		     synthesized_js_event,
		     NULL,
		     NULL);
      }

    /*
     * SEND EVENT_KEYDOWN/UP
     */
    previous_js_event_type  = js_event->type;
    previous_js_event_which = js_event->which;
    ET_SendEvent(form_data->context,
		 (LO_Element *) form_data->form,
		 js_event,
		 NULL,
		 NULL);
  }
}

static void
fe_mocha_radio_form_cb (MWContext *context, LO_Element *element,
			int32 event, void *closure, ETEventStatus status)
{
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)closure;
  LO_FormElementStruct *save;

  if (status == EVENT_OK) {
    if (cb->set)
      save = LO_FormRadioSet (context, (LO_FormElementStruct *) element);
    /* it's possible for save to be null here. it's a legal
       return value from LO_FormRadioSet
       */
    if (cb->set && save && save != (LO_FormElementStruct *) element)
      {
	XFE_SetFormElementToggle (context, save, TRUE);
	LO_FormRadioSet (context, save);
      }
    
    XFE_GetFormElementValue (context, (LO_FormElementStruct *) element, False);
  }
  XP_FREE (cb);
}

/**********************************************************************
 */

static void
fe_radio_form_cb (Widget widget, XtPointer closure, XtPointer call_data)

/*
 * description:
 *	This function was registered as an XmNvalueChangedCallback for
 *	a Motif ToggleButton widget created in XFE_GetFormElementInfo().  
 *
 * preconditions:
 *	widget != 0 && closure != 0 && call_data != 0
 *
 * returns:
 *	not applicable
 *
 **********************************************************************/
{
  FEFormData *fed = (FEFormData *) closure;
  XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct *)call_data;
  LO_FormElementStruct *save;

#ifdef DEBUG
  /* check preconditions in debug mode */
  if (widget == 0 || closure == 0 || call_data == 0) {
      fprintf(stderr, private_precondition_format, __FILE__, __LINE__);
      fflush(stderr);
    }
#endif

  if (cb->set)
    save = LO_FormRadioSet (fed->context, fed->form);
  else
    /* Don't allow the user to ever toggle a button off - exactly one
       must be selected at all times. */
    XtVaSetValues (widget, XmNset, True, 0);

  XFE_GetFormElementValue (fed->context, fed->form, False);

  {
    XEvent  *x_event;
    JSEvent *js_event;
    XP_Bool do_geometry = FALSE;
    XmToggleButtonCallbackStruct *cb_closure;

    cb_closure = XP_NEW_ZAP (XmToggleButtonCallbackStruct);
    XP_MEMCPY (cb_closure, cb, sizeof (XmToggleButtonCallbackStruct));

    x_event  = cb_closure->event;
    js_event = XP_NEW_ZAP(JSEvent);
    js_event->type = EVENT_CLICK;
    fe_complete_js_event(js_event, x_event, do_geometry);
    
    ET_SendEvent (fed->context, (LO_Element *) fed->form, js_event,
		  fe_mocha_radio_form_cb, cb_closure);
  }
}

static void
fe_check_form_cb (Widget widget, XtPointer closure, XtPointer call_data)

/*
 * description:
 *	This function was registered as an XmNvalueChangedCallback for
 *	a Motif ToggleButton widget created in XFE_GetFormElementInfo().  
 *
 * preconditions:
 *	widget != 0 && closure != 0 && call_data != 0
 *
 * returns:
 *	not applicable
 *
 **********************************************************************/
{
  FEFormData *form_data = (FEFormData *) closure;
  XEvent  *x_event;  /* X Windows event structure */
  JSEvent *js_event;
  XP_Bool do_geometry = FALSE;
  lo_FormElementToggleData *data;
  Bool save;

#ifdef DEBUG
  /* check preconditions in debug mode */
  if (widget == 0 || closure == 0 || call_data == 0) {
    fprintf(stderr, private_precondition_format, __FILE__, __LINE__);
    fflush(stderr);
  }
#endif

  data = &form_data->form->element_data->ele_toggle;
  save = data->toggled;

  XFE_GetFormElementValue (form_data->context, form_data->form, False);

  /*
   * The Motif documentation for XmToggleButton informs that call_data
   * is a pointer to XmToggleButtonCallbackStruct, declared in Xm/Xm.h.
   */
  x_event = ((XmToggleButtonCallbackStruct*)call_data)->event;
  js_event = XP_NEW_ZAP(JSEvent);
  js_event->type = EVENT_CLICK;
  fe_complete_js_event(js_event, x_event, do_geometry);

  ET_SendEvent (form_data->context,
		(LO_Element *) form_data->form,
		js_event,
		NULL,
		NULL);
}

static void
fe_combo_form_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
  FESelectOneFormData *sel_fed = (FESelectOneFormData *) closure;
  DtComboBoxCallbackStruct *cb = (DtComboBoxCallbackStruct *) call_data;
  LO_FormElementData *form_data;
  lo_FormElementOptionData *option_data;
  int i;

  form_data = XP_GetFormElementData(sel_fed->form_data.form);

  for (i = 0; i < sel_fed->nkids; i++)
    {
      option_data = XP_FormSelectGetOption(form_data,
					   i);

      /* combo boxes start the item ordering at 0, not 1. */
      sel_fed->selected_p [i] = (i == cb->item_position);
      
      XP_FormOptionSetSelected(option_data, sel_fed->selected_p[i]);
    }
  {
	  JSEvent *event = XP_NEW_ZAP(JSEvent);

	  event->type = EVENT_CHANGE;

	  ET_SendEvent (sel_fed->form_data.context,
			(LO_Element *) sel_fed->form_data.form,
			event,
			NULL, NULL);
  }
}

static void
fe_list_form_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  FESelectMultFormData *sel_fed = (FESelectMultFormData *) closure;
  XmListCallbackStruct *cb = (XmListCallbackStruct *) call_data;
  LO_FormElementData *form_data;
  lo_FormElementOptionData *option_data;
  int i, j;

  form_data = XP_GetFormElementData(sel_fed->form_data.form);

  switch (cb->reason)
    {
    case XmCR_SINGLE_SELECT:
    case XmCR_BROWSE_SELECT:
      for (i = 0; i < sel_fed->nkids; i++)
	{
	  option_data = XP_FormSelectGetOption(form_data,
					       i);

	  /* Note that the item_position starts at 1, not 0!!!! */
	  sel_fed->selected_p [i] = (cb->selected_item_count &&
				     i == (cb->item_position - 1));

	  XP_FormOptionSetSelected(option_data, sel_fed->selected_p[i]);
        }
      break;
    case XmCR_MULTIPLE_SELECT:
    case XmCR_EXTENDED_SELECT:
      for (i = 0; i < sel_fed->nkids; i++)
	{
	  /*
	  **
	  ** toshok -- this is extremely gross and inefficient.  must
	  ** revisit it.  XXXXX
	  **
	  */
	  option_data = XP_FormSelectGetOption(form_data,
					       i);
	  
	  sel_fed->selected_p [i] = 0;
	  XP_FormOptionSetSelected(option_data, sel_fed->selected_p[i]);

	  for (j = 0; j < cb->selected_item_count; j++)
	    if (i == (cb->selected_item_positions [j] - 1))
	      {
	        sel_fed->selected_p [i] = 1;
		XP_FormOptionSetSelected(option_data, sel_fed->selected_p[i]);
	      }	
	}
      break;
    case XmCR_DEFAULT_ACTION:
      break;
    default:
      assert (0);
      return;
    }

  {
	  JSEvent *event = XP_NEW_ZAP(JSEvent);

	  event->type = EVENT_CHANGE;

	  ET_SendEvent (sel_fed->form_data.context,
			(LO_Element *) sel_fed->form_data.form,
			event,
			NULL, NULL);
  }
}


void
XFE_ResetFormElement (MWContext *context, LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed = (FEFormData*)XP_FormGetFEData(form_data);
  
  XP_ASSERT(fed);
  if (!fed) return;
  
  if (fed->vtbl.reset_element)
    (*fed->vtbl.reset_element)(fed, form);
}

/* don't *really* need a vtable entry for this one... or maybe we do. */
void
XFE_SetFormElementToggle (MWContext *context, LO_FormElementStruct *form,
			  XP_Bool state)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;
  int32 form_type;

  if (form_data == NULL) {
#ifdef DEBUG_username
    printf ("forms.c:%d; form_data == NULL\n", __LINE__);
#endif
    return;
  }
  
  fed = (FEFormData*)XP_FormGetFEData(form_data);
  form_type = XP_FormGetType(form_data);

  XP_ASSERT(form_type == FORM_TYPE_CHECKBOX ||
	    form_type == FORM_TYPE_RADIO);
  if (form_type != FORM_TYPE_CHECKBOX
      && form_type != FORM_TYPE_RADIO)
    return;

  XtVaSetValues (fed->widget, XmNset, state, 0);
}

void
fe_SetFormsGravity (MWContext *context, int gravity)
{
  Widget *kids;
  Widget area;
  Cardinal nkids = 0;
  XSetWindowAttributes attr;
  unsigned long valuemask;
 

  XtVaGetValues (CONTEXT_DATA (context)->drawing_area,
                 XmNchildren, &kids, XmNnumChildren, &nkids,
                 0);
 
  valuemask = CWBitGravity | CWWinGravity;
  attr.win_gravity = gravity;
  attr.bit_gravity = gravity;
  area = CONTEXT_DATA (context)->drawing_area;
  XChangeWindowAttributes (XtDisplay(area), XtWindow(area), valuemask, &attr);
  while (nkids--)
  {
    if (XtIsManaged (kids[nkids]))
      XChangeWindowAttributes (XtDisplay(kids[nkids]), XtWindow(kids[nkids]),
                valuemask, &attr);
  }
}

void
fe_ScrollForms (MWContext *context, int x_off, int y_off)
{
  Widget *kids;
  Cardinal nkids = 0;

  if (x_off == 0 && y_off == 0)
    return;

  XtVaGetValues (CONTEXT_DATA (context)->drawing_area,
		 XmNchildren, &kids, XmNnumChildren, &nkids,
		 0);
  while (nkids--)
    XtMoveWidget (kids [nkids],
		  _XfeX(kids [nkids]) + x_off,
		  _XfeY(kids [nkids]) + y_off);
}


void
fe_GravityCorrectForms (MWContext *context, int x_off, int y_off)
{
  Widget *kids;
  Cardinal nkids = 0;

  if (x_off == 0 && y_off == 0)
    return;

  XtVaGetValues (CONTEXT_DATA (context)->drawing_area,
                 XmNchildren, &kids, XmNnumChildren, &nkids,
                 0);

  while (nkids--)
  {
    if (XtIsManaged (kids[nkids]))
    {
        _XfeX(kids[nkids]) = _XfeX(kids[nkids]) + x_off;
        _XfeY(kids[nkids]) = _XfeY(kids[nkids]) + y_off;
    }
  }
}

