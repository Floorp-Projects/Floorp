/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   g-html-forms.c --- form elements for html views.
  Created: Chris Toshok <toshok@hungry.com>, 12-Apr-98.
*/

#include "g-html-view.h"
#include "lo_ele.h"
#include "xpform.h"
#include "libmocha.h"
#include "libevent.h"

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
  GtkWidget *widget;
  MozHTMLView *view;
};

typedef struct {
  FEFormData form_data;

  GtkWidget *rowcolumn;
  GtkWidget *browse_button;
  GtkWidget *file_widget;
} FEFileFormData;

typedef struct {
  FEFormData form_data;

  GtkWidget *text_widget; /* form_data.widget is the scrolled window. */
} FETextAreaFormData;

typedef struct {
  FEFormData form_data;

  GtkWidget *list_widget; /* form_data.widget is the scrolled window. */
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

static void
text_create_widget(FEFormData *fed,
		   LO_FormElementStruct *form)
{

  MWContext *context = MOZ_VIEW(fed->view)->context;
  LO_FormElementData *form_data;
  int32 form_type;
  int32 text_size;
  int32 max_size;
  LO_TextAttr *text_attr;
  int16 charset;
  char *text;

  form_data = XP_GetFormElementData(form);
  form_type = XP_FormGetType(form_data);
  text_size = XP_FormTextGetSize(form_data);
  max_size = XP_FormTextGetMaxSize(form_data);
  text_attr = XP_GetFormTextAttr(form);
  text = (char*)XP_FormGetDefaultText(form_data);
  charset = text_attr->charset;

  fed->widget = gtk_entry_new();

  gtk_entry_set_text(GTK_ENTRY(fed->widget), text);
}


static void
text_display(FEFormData *fed,
	     LO_FormElementStruct *form)
{
  int32 form_x, form_y;

  printf ("displaying text form element\n");

  form_x = form->x + form->x_offset - fed->view->doc_x + fed->view->drawable->x_origin;  
  form_y = form->y + form->y_offset - fed->view->doc_y + fed->view->drawable->y_origin;

  printf ("  moving form element to %d,%d\n", form_x, form_y);

  if (fed->widget->parent == NULL)
    gtk_fixed_put(GTK_FIXED(MOZ_VIEW(fed->view)->subview_parent),
                  fed->widget,
                  form_x,
                  form_y);
  else
    gtk_widget_set_uposition(fed->widget,
                             form_x,
                             form_y);

  gtk_widget_show(fed->widget);
}


static void
text_get_value(FEFormData *fed,
	       LO_FormElementStruct *form,
	       XP_Bool delete_p)
{
  MWContext *context = MOZ_VIEW(fed->view)->context;
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  char *text = 0;
  int32 form_type;
  PA_Block cur_current;
  PA_Block default_txt;

  form_type = XP_FormGetType(form_data);

  if (form_type == FORM_TYPE_TEXT)
      text = gtk_entry_get_text(GTK_ENTRY(fed->widget));
  else if (form_type == FORM_TYPE_PASSWORD)
    abort(); /* XXXX */

  cur_current = XP_FormGetCurrentText(form_data);
  default_txt = XP_FormGetDefaultText(form_data);

  if (cur_current && cur_current != default_txt)
    free ((char*)cur_current);

  XP_FormSetCurrentText(form_data, (PA_Block)text);

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
  GtkEntry *entry = GTK_ENTRY(fed->widget);
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

  gtk_entry_set_position(entry, 0);

  gtk_entry_set_text(entry, default_text);

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

  if (!text) text = (char*)XP_FormGetDefaultText(form_data);

  if (form_type == FORM_TYPE_TEXTAREA)
    abort(); /* XXX */
  else
    gtk_entry_set_text(GTK_ENTRY(fed->widget), text);
}

static void
text_focus(FEFormData *fed,
	   LO_FormElementStruct *form)
{
  gtk_widget_grab_focus(fed->widget);
}


static void
text_lost_focus(FEFormData *fed)
{
  FETextAreaFormData *ta_fed = (FETextAreaFormData*)fed;
  LO_FormElementData *form_data;
  char *text;
  XP_Bool text_changed = FALSE;
  PA_Block current_text;
  JSEvent *event;

  form_data = XP_GetFormElementData(fed->form);

  current_text = XP_FormGetCurrentText(form_data);

  text = gtk_entry_get_text(GTK_ENTRY(fed->widget));

  if (!current_text || XP_STRCMP((char*)current_text, text))
	text_changed = FALSE;

  if (((char*) current_text) != text) {
    free (text);
  }

  /* if the text has changed, call get_element_value to copy it into the form
     element, and send a CHANGE event to the javascript thread. */
  if (text_changed)
	{
		  (*fed->vtbl.get_element_value)(fed, fed->form, FALSE);

		  event = XP_NEW_ZAP(JSEvent);

		  event->type = EVENT_CHANGE;

		  ET_SendEvent (MOZ_VIEW(fed->view)->context, (LO_Element *) fed->form, event,
				NULL, NULL);
	  }
}


static void
text_select(FEFormData *fed,
	    LO_FormElementStruct *form)
{
  printf("text_select (empty)\n");
}

static void
file_create_widget(FEFormData *fed,
		   LO_FormElementStruct *form)
{
	printf("file_create_widget: (not done! will crash)\n");
}

static void
file_get_value(FEFormData *fed,
	       LO_FormElementStruct *form,
	       XP_Bool delete_p)
{
  printf("file_get_value (empty)\n");
}

static void
file_free(FEFormData *fed,
	  LO_FormElementData *form_data)
{
  printf("file_free (empty)\n");
}

void
button_create_widget(FEFormData *fed,
		     LO_FormElementStruct *form)
{
  LO_FormElementData *form_data;

  form_data = XP_GetFormElementData(form);
  fed->widget = gtk_button_new_with_label((unsigned char*)XP_FormGetValue(form_data));
}

void
checkbox_create_widget(FEFormData *fed,
		       LO_FormElementStruct *form)
{
	printf("checkbox_create_widget: (not done! will crash)\n");
}

void
checkbox_get_value(FEFormData *fed,
		   LO_FormElementStruct *form,
		   XP_Bool hide)
{
  printf("checkbox_get_value (empty)\n");
}

void
checkbox_change(FEFormData *fed,
		LO_FormElementStruct *form)
{
  printf("checkbox_change (empty)\n");
}

static void
select_create_widget(FEFormData *fed,
		     LO_FormElementStruct *form)
{
	printf("select_create_widget (not done! will crash!)\n");
}

static void
select_get_value(FEFormData *fed,
		 LO_FormElementStruct *form,
		 XP_Bool delete_p)
{
  printf("select_get_value (empty)\n");
}

static void
select_free(FEFormData *fed,
	    LO_FormElementData *form)
{
  printf("select_free (empty)\n");
}

static void
select_reset(FEFormData *fed,
	     LO_FormElementStruct *form)
{
  printf("select_reset (empty)\n");
}

static void
select_change(FEFormData *fed,
	      LO_FormElementStruct *form)
{
  printf("select_change (empty)\n");
}

static void
textarea_create_widget(FEFormData *fed,
		       LO_FormElementStruct *form)
{
	printf("textarea_create_widget (not done! will crash!)\n");
}

static void
textarea_display(FEFormData *fed,
		 LO_FormElementStruct *form)
{
  printf("textarea_display (empty)\n");
}

static void
textarea_get_value(FEFormData *fed,
		   LO_FormElementStruct *form,
		   XP_Bool delete_p)
{
  printf("textarea_get_value (empty)\n");
}

static void
textarea_reset(FEFormData *fed,
	       LO_FormElementStruct *form)
{
  printf("textarea_reset (empty)\n");
}

static void
textarea_lost_focus(FEFormData *fed)
{
  printf("textarea_lost_focus (empty)\n");
}

static void
form_element_display(FEFormData *fed,
		     LO_FormElementStruct *form)
{
  int32 form_x, form_y;

  form_x = form->x + form->x_offset - fed->view->doc_x + fed->view->drawable->x_origin;  
  form_y = form->y + form->y_offset - fed->view->doc_y + fed->view->drawable->y_origin;

  if (fed->widget->parent == NULL)
    gtk_fixed_put(GTK_FIXED(MOZ_VIEW(fed->view)->subview_parent),
                  fed->widget,
                  form_x,
                  form_y);
  else
    gtk_widget_set_uposition(fed->widget,
                             form_x,
                             form_y);

  gtk_widget_show(fed->widget);
}

static void
form_element_get_size(FEFormData *fed,
		      LO_FormElementStruct *form)
{
  form->width = fed->widget->allocation.width;
  form->height = fed->widget->allocation.height;
}

static void
form_element_is_submit(FEFormData *fed,
		       LO_FormElementStruct *form)
{
  printf("form_element_is_submit (empty)\n");
}

static void
form_element_get_value(FEFormData *fed,
		       LO_FormElementStruct *form,
		       XP_Bool delete_p)
{
  if (delete_p)
    {
      gtk_widget_destroy(fed->widget);
      fed->widget = 0;
    }
}

static void
form_element_free(FEFormData *fed,
		  LO_FormElementData *form)
{
  if (fed->widget)
    gtk_widget_destroy(fed->widget);

  fed->widget = 0;

  XP_FREE(fed);
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

void
moz_html_view_get_form_element_info(MozHTMLView *view,
				    LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  MWContext *context = MOZ_VIEW(view)->context;
  FEFormData *fed;

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
  fed->view = view;

  if (!fed->widget)
    {
      XP_ASSERT(fed->vtbl.create_widget_func);

      (*fed->vtbl.create_widget_func)(fed, form);
    }

  XP_ASSERT(fed->vtbl.get_size_func);

  (*fed->vtbl.get_size_func)(fed, form);
}

void
moz_html_view_display_form_element(MozHTMLView *view,
				   LO_FormElementStruct *form)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;

  printf ("MOZ_HTML_VIEW_DISPLAY_FORM_ELEMENT\n");

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);

  XP_ASSERT(fed);
  if (!fed) return;

  if (fed->vtbl.display_element_func)
    (*fed->vtbl.display_element_func)(fed, form);
}

void
moz_html_view_get_form_element_value(MozHTMLView *view,
				     LO_FormElementStruct *form,
				     XP_Bool delete_p)
{
  LO_FormElementData *form_data = XP_GetFormElementData(form);
  FEFormData *fed;

  if (!form_data)
    return;

  fed = (FEFormData*)XP_FormGetFEData(form_data);

  XP_ASSERT(fed);
  if (!fed) return;

  if (fed->vtbl.get_element_value)
    (*fed->vtbl.get_element_value)(fed, form, delete_p);
}


void
moz_html_view_reset_form_element(MozHTMLView *view,
				 LO_FormElementStruct *form_element)
{
  printf("moz_html_view_reset_form_element (empty)\n");
}


void
moz_html_view_set_form_element_toggle(MozHTMLView *view,
				      LO_FormElementStruct *form_element,
				      XP_Bool toggle)
{
  printf("moz_html_view_set_form_element_toggle (empty)\n");
}


void
moz_html_view_free_form_element(MozHTMLView *view,
				LO_FormElementData *form_data)
{
  FEFormData *fed;

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
moz_html_view_blur_input_element(MozHTMLView *view,
				 LO_FormElementStruct *form_element)
{
  printf("moz_html_view_blur_input_element (empty)\n");
}


void
moz_html_view_focus_input_element(MozHTMLView *view,
				  LO_FormElementStruct *form_element)
{
  printf("moz_html_view_focus_input_element (empty)\n");
}


void
moz_html_view_select_input_element(MozHTMLView *view,
				   LO_FormElementStruct *form_element)
{
  printf("moz_html_view_select_input_element (empty)\n");
}


void
moz_html_view_click_input_element(MozHTMLView *view,
				  LO_FormElementStruct *form_element)
{
  printf("moz_html_view_click_input_element (empty)\n");
}


void
moz_html_view_change_input_element(MozHTMLView *view,
				   LO_FormElementStruct *form_element)
{
  printf("moz_html_view_change_input_element (empty)\n");
}


void
moz_html_view_submit_input_element(MozHTMLView *view,
				   LO_FormElementStruct *form_element)
{
  printf("moz_html_view_submit_input_element (empty)\n");
}
