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
   EmbeddedEditorView.cpp -- class definition for the embedded editor class
   Created: Kin Blas <kin@netscape.com>, 01-Jul-98
 */

#ifdef ENDER

#include "Frame.h"
#include "EmbeddedEditorView.h"
#include "DisplayFactory.h"
#include "ViewGlue.h"
#include "il_util.h"
#include "layers.h"

extern "C"
{
MWContext *fe_CreateNewContext(MWContextType type, Widget w,
							  fe_colormap *cmap, XP_Bool displays_html);
void fe_set_scrolled_default_size(MWContext *context);
CL_Compositor *fe_create_compositor (MWContext *context);
void fe_EditorCleanup(MWContext *context);
void fe_DestroyLayoutData(MWContext *context);
void fe_load_default_font(MWContext *context);
void xfe2_EditorInit(MWContext *context);
int fe_add_to_all_MWContext_list(MWContext *context);
int fe_remove_from_all_MWContext_list(MWContext *context);
void DisplayPixmap(MWContext *context, IL_Pixmap* image,
							  IL_Pixmap *mask, PRInt32 x, PRInt32 y,
							  PRInt32 x_offset, PRInt32 y_offset, PRInt32 width,
							  PRInt32 height);
void fe_find_scrollbar_sizes(MWContext *context);
void fe_get_final_context_resources(MWContext *context);
}



XFE_EmbeddedEditorView::XFE_EmbeddedEditorView(XFE_Component *toplevel_component,
			       Widget parent,
			       XFE_View *parent_view,
			       MWContext *context) 
  : XFE_EditorView(toplevel_component, parent, parent_view, context)
{
  XtOverrideTranslations(CONTEXT_DATA(context)->drawing_area,
                         fe_globalData.editor_global_translations);

  if (parent_view)
      parent_view->addView(this);
}

XFE_EmbeddedEditorView::~XFE_EmbeddedEditorView()
{
  XFE_View *parent_view = getParent();

  if (parent_view)
      parent_view->removeView(this);
}

extern "C" Widget
XFE_CreateEmbeddedEditor(Widget parent, int32 wid, int32 ht,
							const char *default_url, MWContext *context)
{
  Widget w;
  MWContext *new_context;

  XFE_Frame *frame = (XFE_Frame *)CONTEXT_DATA(context)->view;
  XFE_View  *view  = frame->widgetToView(parent);

  new_context = fe_CreateNewContext(MWContextEditor, CONTEXT_WIDGET(context),
                      XFE_DisplayFactory::theFactory()->getPrivateColormap(),
                      TRUE);

  EDITOR_CONTEXT_DATA(new_context)->embedded = TRUE;

  ViewGlue_addMapping(frame, new_context);

  fe_init_image_callbacks(new_context);
  fe_InitColormap(new_context);

  XFE_EmbeddedEditorView *eev = new XFE_EmbeddedEditorView(
				frame,
				CONTEXT_DATA(context)->drawing_area,
				view,
				new_context);

  fe_set_scrolled_default_size(new_context);
  eev->show();

  // New Stuff
  fe_get_final_context_resources(new_context);
  fe_find_scrollbar_sizes(new_context);
  fe_InitScrolling(new_context);
  // New Stuff

  new_context->compositor = fe_create_compositor(new_context);

  w = eev->getBaseWidget();
  XtVaSetValues(w,
				XmNwidth, wid,
				XmNheight, ht,
				XmNborderWidth, 1,
				0);

  XtVaSetValues(CONTEXT_DATA(new_context)->scrolled,
				XmNshadowThickness, 1,
				0);


  xfe2_EditorInit(eev->getContext());

  if (!default_url || !*default_url)
      default_url = "about:editfilenew";

  URL_Struct* url = NET_CreateURLStruct(default_url, NET_NORMAL_RELOAD);

  eev->getURL(url);
  return(w);
}

extern "C" MWContext *
XFE_GetEmbeddedEditorContext(Widget w, MWContext *context)
{
  XFE_Frame *frame = (XFE_Frame *)CONTEXT_DATA(context)->view;
  XFE_View  *view  = frame->widgetToView(w);

  return((view) ? view->getContext() : 0);
}

extern "C" void
XFE_DestroyEmbeddedEditor(Widget w, MWContext *context)
{
  XFE_Frame *frame = (XFE_Frame *)CONTEXT_DATA(context)->view;
  XFE_View  *view  = frame->widgetToView(w);
  MWContext *vcontext;

  if (!view)
	  return;

  vcontext = view->getContext();

  XP_RemoveContextFromList(vcontext);
  fe_remove_from_all_MWContext_list(vcontext);

  delete view;

  if (!vcontext)
	  return;

  fe_EditorCleanup(vcontext);
  fe_DestroyLayoutData (vcontext);

  if (vcontext->color_space)
  {
      IL_ReleaseColorSpace(vcontext->color_space);
      vcontext->color_space = NULL;
  }

  SHIST_EndSession(vcontext);

  if (vcontext->compositor)
  {
      CL_DestroyCompositor(vcontext->compositor);
      vcontext->compositor = NULL;
  }

  free(CONTEXT_DATA(vcontext));
  free(vcontext);
}

extern "C" MWContext *
fe_CreateNewContext(MWContextType type, Widget w, fe_colormap *cmap,
					XP_Bool displays_html)
{
	fe_ContextData *fec;
	struct fe_MWContext_cons *cons;
	MWContext *context;

	context = XP_NewContext();

	if (context == NULL)
		return 0;

	cons = XP_NEW_ZAP(struct fe_MWContext_cons);
	if (cons == NULL)
	{
		XP_FREE(context);
		return 0;
	}

	fec = XP_NEW_ZAP (fe_ContextData);

	if (fec == NULL)
	{
		XP_FREE(cons);
		XP_FREE(context);
		return 0;
	}

	context->type = type;
	switch (type)
	{
		case MWContextEditor:
		case MWContextMessageComposition:
			context->is_editor = True;
			break;
		default:
			context->is_editor = False;
			break;
	}

	CONTEXT_DATA (context)           = fec;
	CONTEXT_DATA (context)->colormap = cmap;

    // set image library Callback functions 
    CONTEXT_DATA (context)->DisplayPixmap = (DisplayPixmapPtr)DisplayPixmap;
    CONTEXT_DATA (context)->NewPixmap     = (NewPixmapPtr)NULL;
    CONTEXT_DATA (context)->ImageComplete = (ImageCompletePtr)NULL;

	CONTEXT_WIDGET (context) = w;

	fe_InitRemoteServer (XtDisplay (w));

	/* add the layout function pointers */
	context->funcs = fe_BuildDisplayFunctionTable();
	context->convertPixX = context->convertPixY = 1;
	context->is_grid_cell = FALSE;
	context->grid_parent = NULL;

	/* set the XFE default Document Character set */
	CONTEXT_DATA(context)->xfe_doc_csid = fe_globalPrefs.doc_csid;

	cons->context = context;
	cons->next = fe_all_MWContexts;
	fe_all_MWContexts = cons;
	XP_AddContextToList (context);

	fe_InitIconColors(context);

	XtGetApplicationResources (w,
							   (XtPointer) CONTEXT_DATA (context),
							   fe_Resources, fe_ResourcesSize,
							   0, 0);

	// Use colors from prefs

	LO_Color *color;

	color = &fe_globalPrefs.links_color;
	CONTEXT_DATA(context)->link_pixel = 
		fe_GetPixel(context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.vlinks_color;
	CONTEXT_DATA(context)->vlink_pixel = 
		fe_GetPixel(context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.text_color;
	CONTEXT_DATA(context)->default_fg_pixel = 
		fe_GetPixel(context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.background_color;
	CONTEXT_DATA(context)->default_bg_pixel = 
		fe_GetPixel(context, color->red, color->green, color->blue);

	if (displays_html) {
        Display * dpy;
        int screen;
        double pixels;
        double millimeters;

        /* Determine pixels per point for back end font size calculations. */

        dpy = XtDisplay(w);
        screen = XScreenNumberOfScreen(XtScreen(w));

#define MM_PER_INCH      (25.4)
#define POINTS_PER_INCH  (72.0)

        /* N pixels    25.4 mm    1 inch
         * -------- *  ------- *  ------
         *   M mm       1 inch    72 pts
         */

        pixels      = DisplayWidth(dpy, screen);
        millimeters = DisplayWidthMM(dpy, screen);
        context->XpixelsPerPoint =
            ((pixels * MM_PER_INCH) / millimeters) / POINTS_PER_INCH;

        pixels      = DisplayHeight(dpy,screen);
        millimeters = DisplayHeightMM(dpy, screen);
        context->YpixelsPerPoint =
            ((pixels * MM_PER_INCH) / millimeters) / POINTS_PER_INCH;


        SHIST_InitSession (context);
	
        fe_load_default_font(context);
	}

	/*
	 * set the default coloring correctly into the new context.
	 */
	{
		Pixel unused_select_pixel;
		XmGetColors (XtScreen (w),
					 fe_cmap(context),
					 CONTEXT_DATA (context)->default_bg_pixel,
					 &(CONTEXT_DATA (context)->fg_pixel),
					 &(CONTEXT_DATA (context)->top_shadow_pixel),
					 &(CONTEXT_DATA (context)->bottom_shadow_pixel),
					 &unused_select_pixel);
	}

    // New field added by putterman for increase/decrease font
    context->fontScalingPercentage = 1.0;

    return context;
}

extern "C" int
fe_add_to_all_MWContext_list(MWContext *context)
{
	struct fe_MWContext_cons *cons;

	XP_ASSERT(context);

	if (!context)
		return 0;

	cons = XP_NEW_ZAP(struct fe_MWContext_cons);

	if (!cons)
		return -1;

	cons->context     = context;
	cons->next        = fe_all_MWContexts;
	fe_all_MWContexts = cons;

	return 0;
}

extern "C" int
fe_remove_from_all_MWContext_list(MWContext *context)
{
	struct fe_MWContext_cons *rest, *prev;

	XP_ASSERT(context);

	if (!context)
		return 0;

    for (prev = 0, rest = fe_all_MWContexts ; rest ;
		 prev = rest, rest = rest->next)
	{
		 if (rest->context == context)
			 break;
	}

	XP_ASSERT(rest);

	if (!rest)
		return -1;

	if (prev)
		prev->next = rest->next;
	else
		fe_all_MWContexts = rest->next;

	free(rest);

	return 0;
}

#endif /* ENDER */
