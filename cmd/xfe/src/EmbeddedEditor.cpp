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
   EmbeddedEditor.cpp -- class definition for the embedded editor class
   Created: Kin Blas <kin@netscape.com>, 01-Jul-98
 */

#ifdef ENDER

#include "Frame.h"
#include "EmbeddedEditor.h"
#include "DisplayFactory.h"
#include "ViewGlue.h"
#include "il_util.h"
#include "layers.h"
#include "BrowserFrame.h"

#include <Xfe/Chrome.h>

#ifndef NO_WEB_FONTS
#include "Mnfrf.h"
#endif
#include "fonts.h"

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
void fe_find_scrollbar_sizes(MWContext *context);
void fe_get_final_context_resources(MWContext *context);
void fe_EditorDisownSelection(MWContext *context, Time time, Boolean clip_p);
XFE_Frame *fe_getFrameFromContext(MWContext* context);
}

static void chrome_update_cb(XFE_NotificationCenter *nc,
							 XFE_NotificationCenter *obj, void *d, void *d2);
static void command_update_cb(XFE_NotificationCenter *nc,
							 XFE_NotificationCenter *obj, void *d, void *d2);

static ToolbarSpec alignment_menu_spec[] = {
	{ xfeCmdSetAlignmentStyleLeft,	 PUSHBUTTON, &ed_left_group },
	{ xfeCmdSetAlignmentStyleCenter, PUSHBUTTON, &ed_center_group },
	{ xfeCmdSetAlignmentStyleRight,	 PUSHBUTTON, &ed_right_group },
	{ NULL }
};

static ToolbarSpec goodies_menu_spec[] = {
	{ xfeCmdInsertLink,           PUSHBUTTON, &ed_link_group   },
	{ xfeCmdInsertTarget,         PUSHBUTTON, &ed_target_group },
	{ xfeCmdInsertImage,          PUSHBUTTON, &ed_image_group  },
	{ xfeCmdInsertHorizontalLine, PUSHBUTTON, &ed_hrule_group  },
	{ xfeCmdInsertTable,          PUSHBUTTON, &ed_table_group  },
	{ NULL }
};

static ToolbarSpec editor_style_toolbar_spec[] = {

//	{ xfeCmdSetParagraphStyle, COMBOBOX },
	{ xfeCmdSetFontFace,       COMBOBOX },
	{ xfeCmdSetFontSize,       COMBOBOX },
	{ xfeCmdSetFontColor,      COMBOBOX },
	TOOLBAR_SEPARATOR,

	{ xfeCmdToggleCharacterStyleBold,	   TOGGLEBUTTON, &ed_bold_group },
	{ xfeCmdToggleCharacterStyleItalic,	   TOGGLEBUTTON, &ed_italic_group },
//	{ xfeCmdToggleCharacterStyleUnderline, TOGGLEBUTTON, &ed_underline_group },
//	{ xfeCmdClearAllStyles,                PUSHBUTTON  , &ed_clear_group },
	TOOLBAR_SEPARATOR,

	{ xfeCmdInsertBulletedList,	TOGGLEBUTTON, &ed_bullet_group },
	{ xfeCmdInsertNumberedList,	TOGGLEBUTTON, &ed_number_group },
	TOOLBAR_SEPARATOR,

	{ xfeCmdOutdent,	PUSHBUTTON, &ed_outdent_group },
	{ xfeCmdIndent,	PUSHBUTTON, &ed_indent_group },
	{ xfeCmdSetAlignmentStyle, CASCADEBUTTON, &ed_left_group, 0, 0, 0,
	  (MenuSpec*)&alignment_menu_spec },
	{ "editorGoodiesMenu", CASCADEBUTTON, &ed_insert_group, 0, 0, 0,
	  (MenuSpec*)&goodies_menu_spec },
	{ NULL }
};

XFE_EmbeddedEditor::XFE_EmbeddedEditor(XFE_Component *toplevel_component,
			       Widget parent,
			       XFE_View *parent_view,
			       int32 cols,
			       int32 rows,
			       const char *default_url,
			       MWContext *context) 
  : XFE_View(toplevel_component, parent_view, context)
{
  Widget chrome;
  XFE_Frame *frame = fe_getFrameFromContext(context);

  XP_ASSERT(frame);

  m_editorContext = fe_CreateNewContext(MWContextEditor,
                      CONTEXT_WIDGET(context),
                      XFE_DisplayFactory::theFactory()->getPrivateColormap(),
                      TRUE);

  EDITOR_CONTEXT_DATA(m_editorContext)->embedded = TRUE;

  ViewGlue_addMapping(frame, m_editorContext);

  fe_init_image_callbacks(m_editorContext);
  fe_InitColormap(m_editorContext);

  chrome = XtVaCreateWidget("EditorChrome", xfeChromeWidgetClass, parent,
							XmNusePreferredWidth,   False,
							XmNusePreferredHeight,   False,
							NULL);
  XtManageChild(chrome);

  setBaseWidget(chrome);
  installDestroyHandler();

  m_toolbox    = new XFE_Toolbox(this, chrome);
  m_toolbar    = new XFE_EditorToolbar(this, m_toolbox,
                                       "editorFormattingToolbar",
                                       (ToolbarSpec*)&editor_style_toolbar_spec,
                                       True);

  m_editorView = new XFE_EmbeddedEditorView(
				toplevel_component,
				chrome,
				this,
				m_editorContext);

  XtVaSetValues(chrome,XmNcenterView, m_editorView->getBaseWidget(),NULL);

  fe_set_scrolled_default_size(m_editorContext);

  // We need to make sure the view is realized before
  // calling fe_InitScrolling() to avoid crashing.
  XtRealizeWidget(m_editorView->getBaseWidget());

  // XtRealizeWidget(m_toolbox->getBaseWidget());
  // if (! XtIsRealized(chrome))
  //   XtRealizeWidget(chrome);

  fe_get_final_context_resources(m_editorContext);
  fe_find_scrollbar_sizes(m_editorContext);
  fe_InitScrolling(m_editorContext);

  m_editorContext->compositor = fe_create_compositor(m_editorContext);

  // Multipliers to get from rows/columns to pixels.
  int cols2pixels, rows2pixels;

  /* get some idea of the size of the default font */
  int16 charset;
  // For some reason, the next line needs a cast otherwise it complains
  // that an object of type void* can't be assigned to an entity of
  // type fe_Font*; that even though fe_LoadFontFromFace() is declared
  // in fonts.h (which we include) to return fe_Font*!
  fe_Font* font = (fe_Font*)fe_LoadFontFromFace(context, NULL, &charset,
                                                0, 3, 0);
  if (font)
  {
    XCharStruct overall;
    int ascent, descent;
    FE_TEXT_EXTENTS(charset, font, "n", 1, &ascent, &descent, &overall);
    cols2pixels = overall.width;
    rows2pixels = ascent + descent;
    /* could also use CONTEXT_DATA(context)->line_height */
  }
  else
  {
    cols2pixels = 7;
    rows2pixels = 17;
#ifdef DEBUG_akkana
    printf("Couldn't get font -- defaulting to %d x %d\n",
           cols2pixels, rows2pixels);
#endif
  }
    
  Dimension wid = cols*cols2pixels
                  + CONTEXT_DATA(m_editorView->getContext())->sb_w;
  Dimension ht  = (rows+1)*rows2pixels
                  + CONTEXT_DATA(m_editorView->getContext())->sb_h;

  XtVaSetValues(m_editorView->getBaseWidget(),
				XmNwidth, wid,
				XmNheight, ht,
				0);

  Dimension tbht = 0;

  // XXX: The size of the viewable area in ENDER is calculated using
  //      the row and column attributes of the TextArea tag. The total size
  //      of an ENDER is the viewable area plus the height of the toolbox.
  //
  //      At this point, the toolbox may not have the correct height.
  //      We need to either fix this or figure out some other way of getting
  //      it's height.
  //
  //      -Kin

  if (m_toolbox)
  {
      XtVaSetValues(m_toolbox->getBaseWidget(),
                    XmNwidth, wid,
                    0);
      XtVaGetValues(m_toolbox->getBaseWidget(),
                    XmNheight, &tbht,
                    0);
  }

  XtVaSetValues(chrome,
				XmNwidth,  wid,
				XmNheight, ht + tbht,
				0);

  if (m_toolbar)
  {
      m_toolbar->setCommandDispatcher(m_editorView);
      m_toolbar->setShowing(TRUE);
      m_toolbar->setOpen(TRUE);
      m_toolbar->setPosition(0);
  }

  if (m_toolbox)
  {
      m_toolbox->show();
      showToolbar();
  }

  m_editorView->show();

  xfe2_EditorInit(m_editorView->getContext());

  if (!default_url || !*default_url)
      default_url = "about:editfilenew";

  URL_Struct* url = NET_CreateURLStruct(default_url, NET_NORMAL_RELOAD);

  m_editorView->getURL(url);

#ifdef BROWSER_FRAME_SUPPORTS_ENDER_TOOLBAR

  // Show the editor toolbars in the containing Frame:
  // XXX NOTE!  This will need to be redone when we support
  // an embedded editor inside an EditorFrame.

  if (!m_toolbox || !m_toolbar)
  {
    XFE_BrowserFrame* bf = (XFE_BrowserFrame*)frame;
    if (bf)
      bf->showEditorToolbar(m_editorView);
  }

#endif /* BROWSER_FRAME_SUPPORTS_ENDER_TOOLBAR */

  if (parent_view)
      parent_view->addView(this);

  toplevel_component->registerInterest(XFE_View::chromeNeedsUpdating,
							           this, chrome_update_cb);

  toplevel_component->registerInterest(XFE_View::commandNeedsUpdating,
							           this, command_update_cb);
}

XFE_EmbeddedEditor::~XFE_EmbeddedEditor()
{
  m_toplevel->unregisterInterest(XFE_View::chromeNeedsUpdating,
							     this, chrome_update_cb);

  m_toplevel->unregisterInterest(XFE_View::commandNeedsUpdating,
							     this, command_update_cb);

  // The XFE_EmbeddedEditor destructor is called via the m_widget's
  // destroy callback.
  //
  // The toolbox and toolbar get deleted the same way via their destroy
  // callbacks, so no need to call delete manually:
  //
  // if (m_toolbar) delete m_toolbar;
  // if (m_toobox)  delete m_toolbox;

  if (m_editorView) delete m_editorView;

  XFE_View *parent_view = getParent();

  if (parent_view)
      parent_view->removeView(this);

  if (!m_editorContext)
	  return;

  XP_RemoveContextFromList(m_editorContext);
  fe_remove_from_all_MWContext_list(m_editorContext);

  fe_EditorCleanup(m_editorContext);
  fe_DestroyLayoutData (m_editorContext);

  if (m_editorContext->color_space)
  {
      IL_ReleaseColorSpace(m_editorContext->color_space);
      m_editorContext->color_space = NULL;
  }

  SHIST_EndSession(m_editorContext);

  if (m_editorContext->compositor)
  {
      CL_DestroyCompositor(m_editorContext->compositor);
      m_editorContext->compositor = NULL;
  }

  free(CONTEXT_DATA(m_editorContext));
  free(m_editorContext);
}

MWContext *
XFE_EmbeddedEditor::getEditorContext()
{
  return(m_editorContext);
}

XFE_EmbeddedEditorView *
XFE_EmbeddedEditor::getEditorView()
{
  return(m_editorView);
}

void
XFE_EmbeddedEditor::setScrollbarsActive(XP_Bool /* b */)
{
  /* Do nothing! */
}

void
XFE_EmbeddedEditor::showToolbar()
{
  if (m_toolbar)
  {
    m_toolbar->update();
    m_toolbar->updateCommand(0);
    m_toolbar->show();
  }
}

void
XFE_EmbeddedEditor::hideToolbar()
{
  if (m_toolbar)
    m_toolbar->hide();
}

static void
chrome_update_cb(XFE_NotificationCenter*, XFE_NotificationCenter* obj,
				 void*, void*)
{
  XFE_EmbeddedEditor *ee = (XFE_EmbeddedEditor *)obj;

  // Forward notification to embedded editor children.

  if (ee)
  {
    Widget w = ee->getBaseWidget();
    if (XfeIsAlive(w))
      ee->notifyInterested(XFE_View::chromeNeedsUpdating);
  }
}

static void
command_update_cb(XFE_NotificationCenter*, XFE_NotificationCenter* obj,
				 void*, void*)
{
  XFE_EmbeddedEditor *ee = (XFE_EmbeddedEditor *)obj;

  // Forward notification to embedded editor children.

  if (ee)
  {
    Widget w = ee->getBaseWidget();
    if (XfeIsAlive(w))
      ee->notifyInterested(XFE_View::commandNeedsUpdating);
  }
}

extern "C" XFE_Frame*
fe_getFrameFromContext(MWContext* context)
{
  XFE_Frame *frame = 0;
  while (context)
  {
    frame = ViewGlue_getFrame(context);
    if (frame)
      return frame;
    if (context->grid_parent)
      context = context->grid_parent;
    else
      return 0;
  }
  return 0; /* warning control -- shouldn't actually get here */
}

extern "C" Widget
XFE_CreateEmbeddedEditor(Widget parent, int32 cols, int32 rows,
							const char *default_url, MWContext *context)
{
  XFE_EmbeddedEditor *ee;
  Widget w = 0;

  XFE_Frame *frame = fe_getFrameFromContext(context);

  XP_ASSERT(frame);

  if (!frame)
    return 0;

  XFE_View  *view  = frame->widgetToView(parent);

  XP_ASSERT(view);

  if (!view)
    return 0;

  ee = new XFE_EmbeddedEditor(frame, parent, view, cols, rows,
                              default_url, context);

  if (ee)
	w = ee->getBaseWidget();

  return(w);
}

extern "C" MWContext *
XFE_GetEmbeddedEditorContext(Widget w, MWContext *context)
{
  XFE_Frame *frame = fe_getFrameFromContext(context);

  XP_ASSERT(frame);

  if (!frame)
    return 0;

  XFE_View  *view  = frame->widgetToView(w);

  XP_ASSERT(view);

  if (!view)
    return 0;

  XFE_EmbeddedEditor *ee = (XFE_EmbeddedEditor *)view;

  return(ee->getEditorContext());
}

extern "C" void
XFE_DestroyEmbeddedEditor(Widget w, MWContext *context)
{
  XFE_Frame *frame = fe_getFrameFromContext(context);

  XP_ASSERT(frame);

  if (!frame)
    return;

  XFE_View  *view  = frame->widgetToView(w);

  XP_ASSERT(view);

  // Delete the view's base widget. The widget's destroy callback will take
  // care of destroying the view itself.

  if (view)
  {
	XFE_EmbeddedEditor *ee   = (XFE_EmbeddedEditor *)view;
    Widget w                 = ee->getBaseWidget();
    MWContext *editorContext = ee->getEditorContext();

    if (editorContext)
      fe_EditorDisownSelection(editorContext, CurrentTime, False);

    if (w)
      XtDestroyWidget(w);
  }
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
    CONTEXT_DATA (context)->DisplayPixmap = (DisplayPixmapPtr)fe_DisplayPixmap;
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
