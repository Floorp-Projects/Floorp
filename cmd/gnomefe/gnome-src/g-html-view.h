/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
  g-html-view.h -- html views.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#ifndef _moz_html_view_h
#define _moz_html_view_h

#include "g-view.h"

/* Platform-specific part of a compositor drawable, which consists of
   a drawing target, an XY offset and a clipping region. */
typedef struct fe_Drawable 
{
    GdkDrawable *drawable;         /* gdk drawable */
    int drawable_serial_num;   /* Serial number for offscreen pixmap */
    int32 x_origin;
    int32 y_origin;
    FE_Region clip_region;
} fe_Drawable;

struct _MozHTMLView {
  /* our superclass */
  MozView _view;

  GdkGC *gc;
  fe_Drawable *drawable;

  GtkObject *vadj, *hadj;

  int32 doc_width, doc_height;
  int32 doc_x, doc_y;
  int32 sw_width, sw_height;

  GtkWidget *scrolled_window;
  int s_width, s_height, s_depth;
};

extern void		moz_html_view_init(MozHTMLView *view, MozFrame *parent_frame, MWContext *context);
extern void		moz_html_view_deinit(MozHTMLView *view);

extern MozHTMLView*	moz_html_view_create(MozFrame *parent_frame, MWContext *context);

extern void moz_html_view_finished_layout(MozHTMLView *view);
extern void moz_html_view_layout_new_document(MozHTMLView *view,
                                              URL_Struct *url,
                                              int32 *iWidth,
                                              int32 *iHeight,
                                              int32 *mWidth,
                                              int32 *mHeight);

extern void moz_html_view_erase_background(MozHTMLView *view,
                                          int32 x, int32 y,
                                          uint32 width, uint32 height,
                                          LO_Color *bg);

extern int moz_html_view_get_text_info(MozHTMLView *view,
                                       LO_TextStruct *text,
                                       LO_TextInfo *info);

extern void moz_html_view_set_doc_dimension(MozHTMLView *view,
                                            int32 iWidth,
                                            int32 iHeight);

extern void moz_html_view_set_doc_position(MozHTMLView *view,
                                           int32 iX,
                                           int32 iY);

extern void moz_html_view_display_text(MozHTMLView *view,
                                       LO_TextStruct *text,
                                       XP_Bool need_bg);
extern void moz_html_view_display_hr(MozHTMLView *view,
                                     LO_HorizRuleStruct *hr);
extern void moz_html_view_display_bullet(MozHTMLView *view,
                                         LO_BulletStruct *bullet);
extern void moz_html_view_display_cell(MozHTMLView *view,
                                       LO_CellStruct *cell);
extern void moz_html_view_display_table(MozHTMLView *view,
                                        LO_TableStruct *table);

extern void moz_html_view_refresh_rect(MozHTMLView *view,
                                       int32 x,
                                       int32 y,
                                       int32 width,
                                       int32 height);

extern XP_Bool moz_html_view_add_image_callbacks(MozHTMLView *view);

extern CL_Compositor *moz_html_view_create_compositor(MozHTMLView* view);

extern void moz_html_set_drawable(MozHTMLView *view,
                                  CL_Drawable *drawable);

extern void moz_html_view_set_background_color(MozHTMLView *view,
                                               uint8 red, uint8 green, uint8 blue);

extern void moz_html_view_get_form_element_info(MozHTMLView *view,
                                                LO_FormElementStruct *form_element);
extern void moz_html_view_get_form_element_value(MozHTMLView *view,
                                                 LO_FormElementStruct *form_element,
                                                 XP_Bool hide);
extern void moz_html_view_reset_form_element(MozHTMLView *view,
                                             LO_FormElementStruct *form_element);
extern void moz_html_view_set_form_element_toggle(MozHTMLView *view,
                                                  LO_FormElementStruct *form_element,
                                                  XP_Bool toggle);
extern void moz_html_view_free_form_element(MozHTMLView *view,
                                            LO_FormElementData *form_data);
extern void moz_html_view_blur_input_element(MozHTMLView *view,
                                             LO_FormElementStruct *form_element);
extern void moz_html_view_focus_input_element(MozHTMLView *view,
                                              LO_FormElementStruct *form_element);
extern void moz_html_view_select_input_element(MozHTMLView *view,
                                               LO_FormElementStruct *form_element);
extern void moz_html_view_click_input_element(MozHTMLView *view,
                                              LO_FormElementStruct *form_element);
extern void moz_html_view_change_input_element(MozHTMLView *view,
                                               LO_FormElementStruct *form_element);
extern void moz_html_view_submit_input_element(MozHTMLView *view,
                                               LO_FormElementStruct *form_element);
extern void moz_html_view_display_form_element(MozHTMLView *view,
                                               LO_FormElementStruct *form_element);

#endif /*_moz_html_view_h */
