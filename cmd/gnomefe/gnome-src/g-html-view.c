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
  g-html-view.c -- html views.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#include "xp_mem.h"
#include "g-html-view.h"

/* Layering support - LO_RefreshArea is called through compositor */
#include "layers.h"
#include "xp_obs.h"
#include "libimg.h"
#include "il_util.h"
#include "prtypes.h"
#include "proto.h"

#include "libevent.h"
#include "g-html-view.h"
#include "g-util.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#if 0
#define DA_EVENTS (GDK_EXPOSURE_MASK        |\
		   GDK_STRUCTURE_MASK	    |\
		   GDK_KEY_PRESS_MASK       |\
		   GDK_KEY_RELEASE_MASK     |\
		   GDK_BUTTON_PRESS_MASK    |\
		   GDK_BUTTON_RELEASE_MASK  |\
		   GDK_LEAVE_NOTIFY_MASK    |\
		   GDK_BUTTON1_MOTION_MASK  |\
		   GDK_POINTER_MOTION_MASK)
#else
#define DA_EVENTS  GDK_ALL_EVENTS_MASK
#endif

#ifdef DEBUG_toshok
#define WANT_SPEW
#endif

#define C8to16(c)           ((((c) << 8) | (c)))

extern gint gnomefe_depth;
extern GdkVisual *gnomefe_visual;

static MWContext *last_documented_context;
static LO_Element *last_documented_element;

static gint
html_view_size_allocate(GtkWidget *widget,
                        GtkAllocation *alloc,
                        void *data);

static gint
html_view_draw(GtkWidget *widget,
               GdkRectangle *area,
               void *data);

static gint
html_view_realize_handler(GtkWidget *widget,
			  void *data)
{
  MozHTMLView *view = (MozHTMLView*)data;
  MWContext *context = MOZ_VIEW(view)->context;
  
  /* Tell the image library to only ever return us depth 1 il_images.
     We will still enlarge these to visual-depth when displaying them. */
  if (!context->color_space)
    {
      context->color_space = IL_CreateGreyScaleColorSpace(1, 1);
      printf("No cs in html_view_realize_handler. Created one\n");
    }
  printf("Got cs for %p (%p)\n", context, context->color_space);

  IL_AddRefToColorSpace(context->color_space);

  context->compositor = moz_html_view_create_compositor(view);

  moz_html_view_add_image_callbacks(view);
  {
    IL_DisplayData dpy_data;
    uint32 display_prefs;
    
    
    dpy_data.color_space = context->color_space;
    dpy_data.progressive_display = 0;
    display_prefs = IL_COLOR_SPACE | IL_PROGRESSIVE_DISPLAY | IL_DITHER_MODE;
    
    IL_SetDisplayMode (context->img_cx, display_prefs, &dpy_data);
  }
  /* add the callback after the widget has been realized and the compositor
     has been created above */
  printf("connecting size_realloc to context %x\n", context);
  gtk_signal_connect(GTK_OBJECT(MOZ_VIEW(view)->subview_parent), "size_allocate",
                     GTK_SIGNAL_FUNC(html_view_size_allocate), context);
  gtk_signal_connect(GTK_OBJECT(MOZ_VIEW(view)->subview_parent), "draw",
                     GTK_SIGNAL_FUNC(html_view_draw), context);
  return TRUE;
}

static gint
html_view_draw(GtkWidget *widget,
               GdkRectangle *area,
               void *data)
{
  MWContext *context = (MWContext *)data;
  MozHTMLView *view = NULL;

  if (!data)
    return FALSE;
  view = find_html_view(context);
  if (!view)
    return FALSE;
  moz_html_view_refresh_rect(view,
                             area->x,
                             area->y,
                             area->width,
                             area->height);
  /* let any other draws work too */
  return FALSE;
}

static gint
html_view_handle_pointer_motion_part2(MWContext *context, LO_Element *element, int32 event,
				      void *closure, ETEventStatus status)
{
#ifdef WANT_SPEW
  printf ("MADE IT HERE.\n");
#endif
  return TRUE;
}


gint
html_view_handle_pointer_motion_for_layer(MozHTMLView *view,
					  CL_Layer *layer,
					  CL_Event *layer_event)
{
  LO_Element *element;
  GdkCursor *cursor;

  element = LO_XYToElement(MOZ_VIEW(view)->context,
			   layer_event->x,
			   layer_event->y,
			   layer);

  if (element && element->type == LO_TEXT)
    {
      if (element->lo_text.anchor_href
	  && (char*)element->lo_text.anchor_href->anchor)
	{
	  JSEvent *event;

	  cursor = gdk_cursor_new(GDK_HAND2);

	  event = XP_NEW_ZAP(JSEvent);
	  event->type = EVENT_MOUSEOVER;
	  event->x = layer_event->x;
	  event->y = layer_event->y;
	  /* get a valid layer id */
	  event->layer_id = LO_GetIdFromLayer (MOZ_VIEW(view)->context, layer);

#ifdef WANT_SPEW
	  printf ("Sending MOUSEOVER event\n");
#endif

	  ET_SendEvent(MOZ_VIEW(view)->context, element, event, 
		       html_view_handle_pointer_motion_part2, NULL);
	}
      else
	{
	  cursor = NULL;
	}
    }
  else
    {
      cursor = NULL;
    }


  gdk_window_set_cursor(MOZ_COMPONENT(view)->base_widget->window,
			cursor);

  if (cursor)
    gdk_cursor_destroy(cursor);
  return TRUE;
}

static gint
html_view_handle_pointer_motion(MozHTMLView *view,
				GdkEvent *event)
{
  MWContext *context = MOZ_VIEW(view)->context;
  CL_Event layer_event;

  memset(&layer_event, 0, sizeof(CL_Event));

  layer_event.type = CL_EVENT_MOUSE_MOVE;
  layer_event.fe_event = (void*)event;
  layer_event.which = 0;
  layer_event.modifiers = 0;

  if (context->compositor)
    {
      layer_event.x = (int)event->motion.x;
      layer_event.y = (int)event->motion.y;

      CL_DispatchEvent(context->compositor, &layer_event);
    }
  else
    {
      html_view_handle_pointer_motion_for_layer(view,
						NULL,
						&layer_event);
    }
  return TRUE;
}

void
html_view_handle_button_press_for_layer(MozHTMLView *view,
					CL_Layer *layer,
					CL_Event *layer_event)
{
  MWContext *context = MOZ_VIEW(view)->context;
  long x, y;
  LO_Element *element;

  if (context->compositor)
    CL_GrabMouseEvents(context->compositor, layer);

  x = layer_event->x;
  y = layer_event->y;

  LO_StartSelection (context, x, y, layer);

  element = LO_XYToElement(MOZ_VIEW(view)->context,
			   x,
			   y,
			   layer);

  if (element && element->type == LO_TEXT)
    {
      if (element->lo_text.anchor_href
	  && (char*)element->lo_text.anchor_href->anchor)
	{
	  LO_HighlightAnchor (context, element, TRUE);
	}
    }
}

static void
html_view_handle_button_press(MozHTMLView *view,
			      GdkEvent *event)
{
  MWContext *context = MOZ_VIEW(view)->context;
  CL_Event layer_event;

  memset(&layer_event, 0, sizeof(CL_Event));

  layer_event.type = CL_EVENT_MOUSE_BUTTON_DOWN;

  if (context->compositor)
      {
	layer_event.x = (int)event->motion.x;
	layer_event.y = (int)event->motion.y;
	layer_event.fe_event = event;
	layer_event.fe_event_size = sizeof(GdkEvent);

	CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
	html_view_handle_button_press_for_layer(view, NULL, &layer_event);
      }
}

extern void fe_url_exit (URL_Struct *url, int status, MWContext *context);

static void
html_view_handle_button_release_part2(MWContext *context, LO_Element *element, int32 event,
				      void *closure, ETEventStatus status)
{
  MozHTMLView *view = (MozHTMLView*)closure;
  URL_Struct *url = NET_CreateURLStruct((char*)element->lo_text.anchor_href->anchor, NET_NORMAL_RELOAD);

#ifdef WANT_SPEW
  printf ("I'm HERE!!!!!\n");
#endif

  NET_GetURL(url, FO_CACHE_AND_PRESENT, MOZ_VIEW(view)->context, fe_url_exit);
}

void
html_view_handle_button_release_for_layer(MozHTMLView *view,
					  CL_Layer *layer,
					  CL_Event *layer_event)
{
  MWContext *context = MOZ_VIEW(view)->context;
  LO_Element *element;
  GdkEvent *gdk_event = (GdkEvent*)layer_event->fe_event;

  if (context->compositor)
      CL_GrabMouseEvents(context->compositor, NULL);

  element = LO_XYToElement(MOZ_VIEW(view)->context,
			   layer_event->x,
			   layer_event->y,
			   layer);

  if (element
      && element->type == LO_TEXT
      && element->lo_text.anchor_href
      && (char*)element->lo_text.anchor_href->anchor)
  {
    JSEvent *jsevent = XP_NEW_ZAP(JSEvent);
    
    jsevent->type = EVENT_CLICK;
    
    jsevent->x = layer_event->x;
    jsevent->y = layer_event->y;
    if (layer) {
      jsevent->docx = layer_event->x + CL_GetLayerXOrigin(layer);
      jsevent->docy = layer_event->y + CL_GetLayerYOrigin(layer);
    }
    else {
      jsevent->docx = layer_event->x;
      jsevent->docy = layer_event->y;
    }
    jsevent->which = layer_event->which;
    jsevent->modifiers = layer_event->modifiers;
    jsevent->screenx = (int)gdk_event->button.x_root;
    jsevent->screeny = (int)gdk_event->button.y_root;

#ifdef WANT_SPEW
    printf ("sending event\n");
#endif

    ET_SendEvent (context,
		  element,
		  jsevent,
		  html_view_handle_button_release_part2,
		  view);
  }
}

static void
html_view_handle_button_release(MozHTMLView *view,
				GdkEvent *event)
{
  MWContext *context = MOZ_VIEW(view)->context;
  CL_Event layer_event;

  layer_event.type = CL_EVENT_MOUSE_BUTTON_UP;

  if (context->compositor)
      {
	layer_event.x = (int)event->motion.x;
	layer_event.y = (int)event->motion.y;
	layer_event.fe_event = event;
	layer_event.fe_event_size = sizeof(GdkEvent);

	CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
	html_view_handle_button_release_for_layer(view, NULL, &layer_event);
      }
}

static gint
html_view_event_handler(GtkWidget *widget,
			GdkEvent *event,
			void *data)
{
  MozHTMLView *view = (MozHTMLView*)data;
  MWContext *context = MOZ_VIEW(view)->context;

  switch (event->type)
    {
    case GDK_EXPOSE:
      {
	moz_html_view_refresh_rect(view,
				   event->expose.area.x,
				   event->expose.area.y,
				   event->expose.area.width,
				   event->expose.area.height);
        return TRUE;
        break;
      }
      return TRUE;
      break;
    case GDK_MOTION_NOTIFY:
      html_view_handle_pointer_motion(view, event);
      return TRUE;
      break;
    case GDK_BUTTON_PRESS:
      html_view_handle_button_press(view, event);
      return TRUE;
      break;
    case GDK_BUTTON_RELEASE:
      html_view_handle_button_release(view, event);
      return TRUE;
      break;
    case GDK_2BUTTON_PRESS:
    default:
      return FALSE;
      break;
    }
}

static gint
html_view_size_allocate(GtkWidget *widget,
                        GtkAllocation *alloc,
                        void *data)
{
  MozHTMLView *view = NULL;
  MWContext *window_context = NULL;

  window_context = (MWContext *)data;
  
  printf("size_allocate called with %x as context.\n", data);
  view = find_html_view(window_context);
  if (view->sw_width  != MOZ_VIEW(view)->subview_parent->allocation.width ||
      view->sw_height != MOZ_VIEW(view)->subview_parent->allocation.height )
    {
      view->sw_width = MOZ_VIEW(view)->subview_parent->allocation.width;
      view->sw_height = MOZ_VIEW(view)->subview_parent->allocation.height;
      if (window_context->compositor != NULL)
        CL_ResizeCompositorWindow(window_context->compositor,
                                  view->sw_width,
                                  view->sw_height);
    }
  return FALSE;
}

void 
moz_html_view_init(MozHTMLView *view, MozFrame *parent_frame, MWContext *context)
{
  /* call our superclass's init */
  moz_view_init(MOZ_VIEW(view), parent_frame, context);

  /* then do our stuff. */
  moz_tagged_set_type(MOZ_TAGGED(view),
		      MOZ_TAG_HTML_VIEW);

  MOZ_VIEW(view)->subview_parent = gtk_fixed_new();

  gtk_widget_set_events (MOZ_VIEW(view)->subview_parent,
			 gtk_widget_get_events (MOZ_VIEW(view)->subview_parent) | DA_EVENTS);

  gtk_signal_connect(GTK_OBJECT(MOZ_VIEW(view)->subview_parent), "realize",
		     (GtkSignalFunc)html_view_realize_handler, view);

  gtk_signal_connect(GTK_OBJECT(MOZ_VIEW(view)->subview_parent), "event",
		     (GtkSignalFunc)html_view_event_handler, view);

  view->vadj = gtk_adjustment_new(0, 0, 100, 1, 1, 1);
  view->hadj = gtk_adjustment_new(0, 0, 100, 1, 1, 1);

  view->scrolled_window = gtk_scrolled_window_new(GTK_ADJUSTMENT(view->hadj),
						  GTK_ADJUSTMENT(view->vadj));

  gtk_container_add(GTK_CONTAINER(view->scrolled_window),
                    MOZ_VIEW(view)->subview_parent);

  //  gtk_widget_show(GTK_WIDGET(view->scrolled_window));
  
  gtk_widget_show(MOZ_VIEW(view)->subview_parent);

  moz_component_set_basewidget(MOZ_COMPONENT(view), view->scrolled_window);
}

void 
moz_html_view_deinit(MozHTMLView *view)
{
  /* do our stuff. */

  /* then call our superclass's deinit */
  moz_view_deinit(MOZ_VIEW(view));
}

MozHTMLView*
moz_html_view_create(MozFrame *parent_frame, MWContext *context)
{
  MozHTMLView *view;

  view = XP_NEW_ZAP(MozHTMLView);
  XP_ASSERT(view);
  if (view == NULL) return NULL;

  /* if context == NULL, then we should create a new context.
     this is used for grid cells. */
  moz_html_view_init(view, parent_frame, context);

  return view;
}

MozHTMLView *
moz_html_view_create_grid_child(MozHTMLView *parent_htmlview,
				MWContext *context)
{
  XP_ASSERT(0);
  if (GTK_IS_FIXED(MOZ_VIEW(parent_htmlview)->subview_parent))
    {
      printf("moz_html_view_create_grid_child (empty)\n");
    }
}

void
moz_html_view_finished_layout(MozHTMLView *view)
{
  MWContext *context = MOZ_VIEW(view)->context;

  printf ("moz_html_view_finished_layout\n");
  if (context->compositor)
      CL_ScrollCompositorWindow(context->compositor, 0, 0);
}

void
moz_html_view_layout_new_document(MozHTMLView *view,
				  URL_Struct *url,
				  int32 *iWidth,
				  int32 *iHeight,
				  int32 *mWidth,
				  int32 *mHeight)
{
  printf ("moz_html_view_layout_new_document\n");

  *iWidth = MOZ_VIEW(view)->subview_parent->allocation.width;
  *iHeight = MOZ_VIEW(view)->subview_parent->allocation.height;

  if (*mWidth == 0)
    *mWidth = 8;

  if (*mHeight == 0)
    *mHeight = 8;

  printf ("\treturning (%d,%d)\n", *iWidth, *iHeight);
}

void
moz_html_view_erase_background(MozHTMLView *view,
			       int32 x, int32 y,
			       uint32 width, uint32 height,
			       LO_Color *bg)
{
  fe_Drawable *fe_drawable = view->drawable;
  GdkGC *gc = gdk_gc_new((GdkWindow*)fe_drawable->drawable);
  GdkColor color;

  color.red = C8to16(bg->red);
  color.green = C8to16(bg->green);
  color.blue = C8to16(bg->blue);

  gdk_color_alloc(gtk_widget_get_colormap(MOZ_VIEW(view)->subview_parent),
		  &color);

  gdk_gc_set_foreground(gc, &color);

  gdk_draw_rectangle((GdkWindow*)fe_drawable->drawable,
		     gc, TRUE,
		     -view->doc_x + x,
		     -view->doc_y + y,
		     width, height);

  gdk_gc_destroy(gc);
}

int
moz_html_view_get_text_info(MozHTMLView *view,
			    LO_TextStruct *text,
			    LO_TextInfo *info)
{
  char *text_buf;
  GdkFont *font = moz_get_font(text->text_attr);

  text_buf = (char*)malloc(text->text_len + 1);

  strncpy(text_buf, (char*)text->text, text->text_len);
  text_buf [ text->text_len ] = 0;

  //  printf ("moz_html_view_get_text_info (%s)... ", text_buf);

  info->max_width = gdk_string_width(font, text_buf);

  free(text_buf);

  info->ascent = font->ascent;
  info->descent = font->descent;

  /* These values *must* be set! */
  info->lbearing = 0;
  info->rbearing = 0;

  return 0;
}

void
moz_html_view_set_doc_dimension(MozHTMLView *view,
				int32 iWidth,
				int32 iHeight)
{
  gtk_widget_set_usize(MOZ_VIEW(view)->subview_parent,
		       iWidth,
		       iHeight);

  view->doc_width = iWidth;
  view->doc_height = iHeight;
}

void
moz_html_view_set_doc_position(MozHTMLView *view,
			       int32 iX,
			       int32 iY)
{
  printf ("moz_html_view_set_doc_position(%d,%d)\n", iX, iY);
}

void
moz_html_view_display_text(MozHTMLView *view,
			   LO_TextStruct *text,
			   XP_Bool need_bg)
{
  MWContext *context = MOZ_VIEW(view)->context;
  fe_Drawable *fe_drawable = view->drawable;
  GdkGC *gc = gdk_gc_new((GdkWindow*)fe_drawable->drawable);
  GdkColor color;
  char *text_to_display;
  int32 text_x, text_y;
  GdkFont *font = moz_get_font(text->text_attr);

  color.red = C8to16(text->text_attr->fg.red);
  color.green = C8to16(text->text_attr->fg.green);
  color.blue = C8to16(text->text_attr->fg.blue);

  text_to_display = malloc(text->text_len + 1);
  strncpy(text_to_display, (char*)text->text, text->text_len);
  text_to_display[text->text_len] = 0;

#ifdef WANT_SPEW
  printf ("display text (%s)\n",
	  (char*)text_to_display);
#endif

  gdk_color_alloc(gtk_widget_get_colormap(MOZ_VIEW(view)->subview_parent),
		  &color);

  gdk_gc_set_foreground(gc, &color);

  if (need_bg)
    {
      GdkColor bg_color;

      bg_color.red = text->text_attr->bg.red;
      bg_color.green = text->text_attr->bg.green;
      bg_color.blue = text->text_attr->bg.blue;

      gdk_color_alloc(gtk_widget_get_colormap(MOZ_VIEW(view)->subview_parent),
		      &bg_color);

      gdk_gc_set_background(gc, &bg_color);
    }

  text_x = text->x + text->x_offset - view->doc_x + fe_drawable->x_origin;
  text_y = text->y + text->y_offset - view->doc_y + fe_drawable->y_origin;

  gdk_draw_text ((GdkWindow*)fe_drawable->drawable,
		 font,
		 gc,
		 text_x,
		 text_y + font->ascent,
		 (char*)text_to_display,
		 text->text_len);

  if (text->text_attr->attrmask & LO_ATTR_UNDERLINE)
    {
      gdk_draw_line((GdkWindow*)fe_drawable->drawable,
		    gc,
		    text_x,
		    text_y + font->ascent + 1,
		    text_x + text->width,
		    text_y + font->ascent + 1);
    }

  free(text_to_display);

  gdk_gc_destroy(gc);
}

void
moz_html_view_display_hr(MozHTMLView *view,
			 LO_HorizRuleStruct *hr)
{
  MWContext *context = MOZ_VIEW(view)->context;
  fe_Drawable *fe_drawable = view->drawable;
  GdkGC *gc = gdk_gc_new((GdkWindow*)fe_drawable->drawable);
  GdkColor color;
  int32 hr_x;
  int32 hr_y;

  gdk_color_black(gtk_widget_get_colormap(MOZ_VIEW(view)->subview_parent),
		  &color);

  gdk_gc_set_foreground(gc, &color);
  gdk_gc_set_line_attributes(gc,
			     hr->thickness,
			     GDK_LINE_SOLID,
			     GDK_CAP_NOT_LAST,
			     GDK_JOIN_MITER);

  hr_x = hr->x + hr->x_offset - view->doc_x + fe_drawable->x_origin;
  hr_y = hr->y + hr->y_offset - view->doc_y + fe_drawable->y_origin;

  gdk_draw_line (fe_drawable->drawable,
		 gc,
		 hr_x,
		 hr_y + hr->thickness / 2,
		 hr_x + hr->width,
		 hr_y + hr->thickness / 2);
  
  gdk_gc_destroy(gc);
}

void
moz_html_view_display_bullet(MozHTMLView *view,
			     LO_BulletStruct *bullet)
{
  int w,h;
  XP_Bool hollow;
  fe_Drawable *fe_drawable = view->drawable;
  int32 bullet_x;
  int32 bullet_y;
  GdkDrawable *drawable = fe_drawable->drawable;
  GdkGC *gc = gdk_gc_new((GdkWindow*)drawable);
  GdkColor color;

  color.red = C8to16(bullet->text_attr->fg.red);
  color.green = C8to16(bullet->text_attr->fg.green);
  color.blue = C8to16(bullet->text_attr->fg.blue);

  gdk_color_alloc(gtk_widget_get_colormap(MOZ_VIEW(view)->subview_parent),
		  &color);

  gdk_gc_set_foreground(gc, &color);

  hollow = (bullet->bullet_type != BULLET_BASIC);
  w = bullet->width + 1;
  h = bullet->height + 1;

  bullet_x = bullet->x + bullet->x_offset - view->doc_x + fe_drawable->x_origin;  
  bullet_y = bullet->y + bullet->y_offset - view->doc_y + fe_drawable->y_origin;

  switch (bullet->bullet_type)
    {
    case BULLET_BASIC:
    case BULLET_ROUND:
      /* Subtract 1 to compensate for the behavior of XDrawArc(). */
      w -= 1;
      h -= 1;
      /* Now round up to an even number so that the circles look nice. */
      if (! (w & 1)) w++;
      if (! (h & 1)) h++;
      gdk_draw_arc (drawable, gc, !hollow, bullet_x, bullet_y, w + 1, h + 1, 0, 360*64);
      break;
    case BULLET_SQUARE:
      gdk_draw_rectangle (drawable, gc, !hollow, bullet_x, bullet_y, w + 1, h + 1);
    case BULLET_MQUOTE:
      /*
       * WARNING... [ try drawing a 2 pixel wide filled rectangle ]
       *
       *
       */
      w = 2;
      gdk_draw_rectangle (drawable, gc, TRUE, bullet_x, bullet_y, w, h);
      break;
    case BULLET_NONE:
      /* Do nothing. */
      break;
    default:
      XP_ASSERT(0);
    }

  gdk_gc_destroy(gc);
}

void
moz_html_view_refresh_rect(MozHTMLView *view,
			   int32 x,
			   int32 y,
			   int32 width,
			   int32 height)
{
  if(MOZ_VIEW(view)->context->compositor)
    {
      XP_Rect rect;

      rect.left = x;
      rect.top = y;
      rect.right = x + width;
      rect.bottom = y + height;
      CL_UpdateDocumentRect(MOZ_VIEW(view)->context->compositor, &rect, PR_TRUE);
    }
}

/* Callback to set the XY offset for all drawing into this drawable */
static void
fe_set_drawable_origin(CL_Drawable *drawable, int32 x_origin, int32 y_origin) 
{
  fe_Drawable *fe_drawable = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable);
  fe_drawable->x_origin = x_origin;
  fe_drawable->y_origin = y_origin;
}

/* Callback to set the clip region for all drawing calls */
static void
fe_set_drawable_clip(CL_Drawable *drawable, FE_Region clip_region)
{
  fe_Drawable *fe_drawable = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable);
  fe_drawable->clip_region = clip_region;
}

/* Callback not necessary, but may help to locate errors */
static void
fe_restore_drawable_clip(CL_Drawable *drawable)
{
  fe_Drawable *fe_drawable = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable);
  fe_drawable->clip_region = NULL;
}

void
moz_html_view_set_drawable(MozHTMLView *view,
			   CL_Drawable *drawable)
{
  fe_Drawable *fe_drawable;
  if (! drawable)
    return;

  fe_drawable = (fe_Drawable*)CL_GetDrawableClientData(drawable);
  view->drawable = fe_drawable;
}

/* XXX - Works faster if we don't define this, but seems to introduce bugs */
#define USE_REGION_FOR_COPY

#ifndef USE_REGION_FOR_COPY

static GdkDrawable *fe_copy_src, *fe_copy_dst;
static GdkGC *fe_copy_gc;

static void
fe_copy_rect_func(void *empty, XP_Rect *rect)
{
  printf ("in fe_copy_rect_func\n");

  gdk_window_copy_area ((GdkWindow*)fe_copy_dst,
			fe_copy_gc,
			rect->left, rect->top,
			(GdkWindow*)fe_copy_src,
			rect->left, rect->top,
			rect->right - rect->left, rect->bottom - rect->top);
}
#endif

static void
fe_copy_pixels(CL_Drawable *drawable_src, 
               CL_Drawable *drawable_dst, 
               FE_Region region)
{
  XP_Rect bbox;
  GdkGCValues gcv;
  GdkGC *gc;
  fe_Drawable *fe_drawable_dst = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable_dst);
  fe_Drawable *fe_drawable_src = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable_src);

  GdkDrawable *src = fe_drawable_src->drawable;
  GdkDrawable *dst = fe_drawable_dst->drawable;

  printf ("fe_copy_pixels (src %p, dst %p, region %d\n",
          drawable_src, drawable_dst, region);

#if 0
  /* FIXME - for simple regions, it may be faster to iterate over
     rectangles rather than using clipping regions */ 
  memset (&gcv, ~0, sizeof (gcv));
  gcv.function = GXcopy;
#endif

#ifdef USE_REGION_FOR_COPY
  gcv.graphics_exposures = FALSE;
  gc = gdk_gc_new_with_values(dst,
			      &gcv,
			      GDK_GC_EXPOSURES);

  FE_GetRegionBoundingBox(region, &bbox);

  gdk_window_copy_area ((GdkWindow*)dst,
			gc,
			bbox.left, bbox.top,
			(GdkWindow*)src,
			bbox.left, bbox.top,
			bbox.right - bbox.left, bbox.bottom - bbox.top);
#else
  gcv.graphics_exposures = FALSE;
  fe_copy_gc = gdk_gc_new_with_values(dst,
				      &gcv,
				      GDK_GC_EXPOSURES);
  fe_copy_src = src;
  fe_copy_dst = dst;
  FE_ForEachRectInRegion(region, 
			 (FE_RectInRegionFunc)fe_copy_rect_func, NULL);
#endif
}

/* There is only one backing store pixmap shared among all windows */
static GdkPixmap* fe_backing_store_pixmap = 0;

/* We use a serial number to compare pixmaps rather than the X Pixmap
   handle itself, in case the server allocates a pixmap with the same
   handle as a pixmap that we've deallocated.  */
static int fe_backing_store_pixmap_serial_num = 0;

/* Current lock owner for backing store */
static fe_Drawable *backing_store_owner = NULL;
static int backing_store_width = 0;
static int backing_store_height = 0;
static int backing_store_refcount = 0;
static int backing_store_depth;

static void
fe_destroy_backing_store(CL_Drawable *drawable)
{
  gdk_pixmap_unref(fe_backing_store_pixmap);
  backing_store_owner = NULL;
  fe_backing_store_pixmap = 0;
  backing_store_width = 0;
  backing_store_height = 0;
}

/* Function that's called to indicate that the drawable will be used.
   No other drawable calls will be made until we InitDrawable. */
static void
fe_init_drawable(CL_Drawable *drawable)
{
  backing_store_refcount++;
}
  
/* Function that's called to indicate that we're temporarily done with
   the drawable. The drawable won't be used until we call InitDrawable
   again. */
static void
fe_relinquish_drawable(CL_Drawable *drawable)
{
  XP_ASSERT(backing_store_refcount > 0);
  backing_store_refcount--;

  if (backing_store_refcount == 0)
    fe_destroy_backing_store(drawable);
}

static PRBool
fe_lock_drawable(CL_Drawable *drawable, CL_DrawableState new_state)
{
  fe_Drawable *prior_backing_store_owner;
  fe_Drawable *fe_drawable = (fe_Drawable *)CL_GetDrawableClientData(drawable);
  if (new_state == CL_UNLOCK_DRAWABLE)
    return PR_TRUE;

  XP_ASSERT(backing_store_refcount > 0);

  if (!fe_backing_store_pixmap)
      return PR_FALSE;

  prior_backing_store_owner = backing_store_owner;

  /* Check to see if we're the last one to use this drawable.
     If not, someone else might have modified the bits, since the
     last time we wrote to them using this drawable. */
  if (new_state & CL_LOCK_DRAWABLE_FOR_READ) {
    if (prior_backing_store_owner != fe_drawable)
        return PR_FALSE;

    /* The pixmap could have changed since the last time this
       drawable was used due to a resize of the backing store, even
       though no one else has drawn to it.  */
    if (fe_drawable->drawable_serial_num !=
        fe_backing_store_pixmap_serial_num) {
        return PR_FALSE;
    }
  }

  backing_store_owner = fe_drawable;

  fe_drawable->drawable = (GdkDrawable*)fe_backing_store_pixmap;
  fe_drawable->drawable_serial_num = fe_backing_store_pixmap_serial_num;

  return PR_TRUE;
}

static void
fe_set_drawable_dimensions(CL_Drawable *drawable, uint32 width, uint32 height)
{
  fe_Drawable *fe_drawable = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable);

  printf ("fe_set_drawable_dimensions (draw %p, width %d, height %d)\n", 
	drawable, width, height);

  XP_ASSERT(backing_store_refcount > 0);
  if ((width > backing_store_width) || (height > backing_store_height)) {  
        
    /* The backing store only gets larger, not smaller. */
    if (width < backing_store_width)
      width = backing_store_width;
    if (height < backing_store_height)
      height = backing_store_height;
        
    if (fe_backing_store_pixmap) {
      gdk_pixmap_unref(fe_backing_store_pixmap);
      backing_store_owner = NULL;
    }

    fe_backing_store_pixmap_serial_num++;

    /* Crashes otherwise */
    if (fe_drawable->drawable)
      fe_drawable->drawable =NULL;
    fe_backing_store_pixmap = gdk_pixmap_new(fe_drawable->drawable,
					     width, height,
					     backing_store_depth);

    XP_ASSERT(fe_backing_store_pixmap);
    if (!fe_backing_store_pixmap) abort();

    backing_store_width = width;
    backing_store_height = height;
  }
  fe_drawable->drawable = fe_backing_store_pixmap;
}

static
CL_DrawableVTable window_drawable_vtable = {
    NULL,
    NULL,
    NULL,
    NULL,
    fe_set_drawable_origin,
    NULL,
    fe_set_drawable_clip,
    fe_restore_drawable_clip,
    fe_copy_pixels,
    NULL
};

static
CL_DrawableVTable backing_store_drawable_vtable = {
    fe_lock_drawable,
    fe_init_drawable,
    fe_relinquish_drawable,
    NULL,
    fe_set_drawable_origin,
    NULL,
    fe_set_drawable_clip,
    fe_restore_drawable_clip,
    fe_copy_pixels,
    fe_set_drawable_dimensions
};

CL_Compositor *
moz_html_view_create_compositor(MozHTMLView *view)
{
  int32 comp_width, comp_height;
  CL_Drawable *cl_window_drawable, *cl_backing_store_drawable;
  CL_Compositor *compositor;
  fe_Drawable *window_drawable, *backing_store_drawable;
  GdkWindow *window;
  GdkVisual *visual;

  window = MOZ_VIEW(view)->subview_parent->window;
  visual = gnomefe_visual;
  backing_store_depth = gnomefe_depth;

  comp_width = MOZ_VIEW(view)->subview_parent->allocation.width;
  comp_height = MOZ_VIEW(view)->subview_parent->allocation.height;
  view->sw_height = comp_height;
  view->sw_width = comp_width; 
  printf ("width %d, height %d\n", comp_width, comp_height);
#if 0
  if (CONTEXT_DATA(context)->vscroll && 
      XtIsManaged(CONTEXT_DATA(context)->vscroll))
    comp_width -= CONTEXT_DATA(context)->sb_w;
  if (CONTEXT_DATA(context)->hscroll &&
      XtIsManaged(CONTEXT_DATA(context)->hscroll))
    comp_height -= CONTEXT_DATA(context)->sb_h;
#endif

  window_drawable = XP_NEW_ZAP(fe_Drawable);
  if (!window_drawable)
    return NULL;
  window_drawable->drawable = window;

  /* Create backing store drawable, but don't create pixmap
     until fe_set_drawable_dimensions() is called from the
     compositor */
  backing_store_drawable = XP_NEW_ZAP(fe_Drawable);
  if (!backing_store_drawable)
    return NULL;

  /* Create XP handle to window's HTML view for compositor */
  cl_window_drawable = CL_NewDrawable(comp_width, comp_height, 
				      CL_WINDOW, &window_drawable_vtable,
				      (void*)window_drawable);

  /* Create XP handle to backing store for compositor */
  cl_backing_store_drawable = CL_NewDrawable(comp_width, comp_height, 
					     CL_BACKING_STORE,
					     &backing_store_drawable_vtable,
					     (void*)backing_store_drawable);

  compositor = CL_NewCompositor(cl_window_drawable, cl_backing_store_drawable,
				0, 0, comp_width, comp_height, 15);

  view->drawable = window_drawable;

  return compositor;
}

static void
moz_html_view_image_group_observer(XP_Observable observable, XP_ObservableMsg message,
				   void *message_data, void *closure)
{
  MozHTMLView *view = (MozHTMLView*)closure;

  switch (message) {
  case IL_STARTED_LOADING:
    printf ("IL_STARTED_LOADING\n");
    break;
    
  case IL_ABORTED_LOADING:
    printf ("IL_ABORTED_LOADING\n");
    break;

  case IL_FINISHED_LOADING:
    printf ("IL_FINISHED_LOADING\n");
    break;

  case IL_STARTED_LOOPING:
    printf ("IL_STARTED_LOOPING\n");
    break;

  case IL_FINISHED_LOOPING:
    printf ("IL_FINISHED_LOOPING\n");
    break;
    
  default:
    break;
  }

  FE_UpdateStopState(MOZ_VIEW(view)->context);
}

XP_Bool
moz_html_view_add_image_callbacks(MozHTMLView *view)
{
    IL_GroupContext *img_cx;
    IMGCB* img_cb;
    JMCException *exc = NULL;
    MWContext *context = MOZ_VIEW(view)->context;

    if (!context->img_cx) {
        PRBool observer_added_p;
        
        img_cb = IMGCBFactory_Create(&exc); /* JMC Module */
        if (exc) {
            JMC_DELETE_EXCEPTION(&exc); /* XXXM12N Should really return
                                           exception */
            return FALSE;
        }

        /* Create an Image Group Context.  IL_NewGroupContext augments the
           reference count for the JMC callback interface.  The opaque argument
           to IL_NewGroupContext is the Front End's display context, which will
           be passed back to all the Image Library's FE callbacks. */
        img_cx = IL_NewGroupContext((void*)context, (IMGCBIF *)img_cb);

        /* Attach the IL_GroupContext to the FE's display context. */
        context->img_cx = img_cx;

        /* Add an image group observer to the IL_GroupContext. */
        observer_added_p = IL_AddGroupObserver(img_cx, moz_html_view_image_group_observer,
                                               (void *)view);
    }
    return TRUE;
}

void
moz_html_view_set_background_color(MozHTMLView *view,
				   uint8 red, uint8 green, uint8 blue)
{
  GdkColor color;

  color.red = C8to16(red);
  color.green = C8to16(green);
  color.blue = C8to16(blue);

  gdk_color_alloc(gtk_widget_get_colormap(MOZ_VIEW(view)->subview_parent),
		  &color);

  if (MOZ_VIEW(view)->subview_parent)
    gdk_window_set_background(MOZ_VIEW(view)->subview_parent->window,
			      &color);
}

void
moz_html_view_display_cell(MozHTMLView *view,
			   LO_CellStruct *cell)
{
  MWContext *context = MOZ_VIEW(view)->context;
  fe_Drawable *fe_drawable = view->drawable;
  GdkGC *gc = gdk_gc_new((GdkWindow*)fe_drawable->drawable);
  GdkColor color;
  int32 cell_x, cell_y, border_width;

  cell_x = cell->x + cell->x_offset - view->doc_x + fe_drawable->x_origin;  
  cell_y = cell->y + cell->y_offset - view->doc_y + fe_drawable->y_origin;
  border_width = cell->border_width;
  if (!view)
    return;

  if ((cell_x > 0 && cell_x > view->sw_width) ||
      (cell_y > 0 && cell_y > view->sw_height) ||
      (cell_x + cell->width < 0) ||
      (cell_y + cell->line_height < 0)||
      (cell->border_width < 1))
    return;

  gdk_color_black(gtk_widget_get_colormap(MOZ_VIEW(view)->subview_parent),
		  &color);

  gdk_gc_set_foreground(gc, &color);

  gdk_draw_rectangle ((GdkWindow*)fe_drawable->drawable,
		      gc,
		      FALSE,
		      cell_x,
		      cell_y,
		      cell->width, cell->height);

  gdk_gc_destroy(gc);
}

void
moz_html_view_display_table(MozHTMLView *view,
			    LO_TableStruct *table)
{
  XP_ASSERT(0);
  printf("moz_html_view_display_table (empty)\n");
}
