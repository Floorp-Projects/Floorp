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
   gnomecxtx.c --- gnome fe handling of MWContext initialization.
*/

#include "structs.h"
#include "ntypes.h"
#include "xpassert.h"
#include "proto.h"
#include "fe_proto.h"
#include "edt.h"

#include "gnome-form.h"
#include "g-view.h"
#include "g-frame.h"
#include "g-util.h"
#include "gnomefe.h"
#include "g-html-view.h"

#ifndef NETSCAPE_PRIV
#include "../../lib/xp/flamer.h"
#else
#include "../../lib/xp/biglogo.h"
#include "../../lib/xp/photo.h"
#include "../../lib/xp/hype.h"
#ifdef MOZ_SECURITY
#include "../../lib/xp/rsalogo.h"
#include "ssl.h"
#include "xp_sec.h"
#endif
#ifdef JAVA
#include "../../lib/xp/javalogo.h"
#endif
#ifdef FORTEZZA
#include "../../lib/xp/litronic.h"
#endif
#include "../../lib/xp/coslogo.h"
#include "../../lib/xp/insologo.h"
#include "../../lib/xp/mclogo.h"
#include "../../lib/xp/ncclogo.h"
#include "../../lib/xp/odilogo.h"
#include "../../lib/xp/qt_logo.h"
#include "../../lib/xp/tdlogo.h"
#include "../../lib/xp/visilogo.h"
#endif /* !NETSCAPE_PRIV */

static MWContext*
GNOMEFE_CreateNewDocWindow(MWContext *calling_context,
			  URL_Struct *URL)
{
  printf ("GNOMEFE_CreateNewDocWindow\n");
  XP_ASSERT(0);
  return NULL;
}

static void
GNOMEFE_LayoutNewDocument(MWContext *context,
                          URL_Struct *url_struct,
                          int32 *iWidth,
                          int32 *iHeight,
                          int32 *mWidth,
                          int32 *mHeight)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return; /* XXX */

  moz_html_view_layout_new_document(view,
                                    url_struct,
                                    iWidth,
                                    iHeight,
                                    mWidth,
                                    mHeight);
}

static void 
GNOMEFE_SetDocTitle (MWContext * context,
                     char * title)
{
  MozTagged *tagged = (MozTagged*)context->fe.data->frame_or_view;

  if (MOZ_IS_FRAME(tagged))
    moz_frame_set_title(MOZ_FRAME(tagged), title);
}

static void 
GNOMEFE_FinishedLayout (MWContext *context)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return; /* XXX */

  moz_html_view_finished_layout(view);
}

static char* 
GNOMEFE_TranslateISOText (MWContext * context,
			 int charset,
			 char *ISO_Text)
{
  printf ("GNOMEFE_TranslateISOText (%s)\n", ISO_Text);

  return ISO_Text; /* XXXX */
}

static int 
GNOMEFE_GetTextInfo (MWContext * context,
                     LO_TextStruct *text,
                     LO_TextInfo *text_info)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return -1; /* XXX */

  return moz_html_view_get_text_info(view, text, text_info);
}

static void
GNOMEFE_CreateEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
  XP_ASSERT(0);
  printf("GNOMEFE_CreateEmbedWindow (empty)\n");
}

static void
GNOMEFE_SaveEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
  XP_ASSERT(0);
  printf("GNOMEFE_SaveEmbedWindow (empty)\n");
}

static void
GNOMEFE_RestoreEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
  XP_ASSERT(0);
  printf("GNOMEFE_RestoreEmbedWindow (empty)\n");
}

static void
GNOMEFE_DestroyEmbedWindow(MWContext *context, NPEmbeddedApp *app)
{
  XP_ASSERT(0);
  printf("GNOMEFE_DestroyEmbedWindow (empty)\n");
}

static void 
GNOMEFE_GetEmbedSize (MWContext * context,
		     LO_EmbedStruct *embed_struct,
		     NET_ReloadMethod force_reload)
{
  XP_ASSERT(0);
  printf("GNOMEFE_GetEmbedSize (empty)\n");
}

static void 
GNOMEFE_GetJavaAppSize (MWContext * context,
		       LO_JavaAppStruct *java_struct,
		       NET_ReloadMethod force_reload)
{
  XP_ASSERT(0);
  printf("GNOMEFE_GetJavaAppSize (empty)\n");
}

static void 
GNOMEFE_FreeEmbedElement (MWContext *context,
			 LO_EmbedStruct *embed)
{
  XP_ASSERT(0);
  printf("GNOMEFE_FreeEmbedElement (empty)\n");
}

static void 
GNOMEFE_FreeJavaAppElement (MWContext *context,
			   struct LJAppletData *appletData)
{
  XP_ASSERT(0);
  printf("GNOMEFE_FreeJavaAppElement (empty)\n");
}

static void 
GNOMEFE_HideJavaAppElement (MWContext *context,
			   struct LJAppletData *java_app)
{
  XP_ASSERT(0);
  printf("GNOMEFE_HideJavaAppElement (empty)\n");
}

static void 
GNOMEFE_FreeEdgeElement (MWContext *context,
			LO_EdgeStruct *edge)
{
  XP_ASSERT(0);
  printf("GNOMEFE_FreeEdgeElement (empty)\n");
}

static void 
GNOMEFE_FormTextIsSubmit (MWContext * context,
                          LO_FormElementStruct * form_element)
{
  XP_ASSERT(0);
  printf("GNOMEFE_FormTextIsSubmit (empty)\n");
}

static void 
GNOMEFE_DisplaySubtext (MWContext * context,
		       int iLocation,
		       LO_TextStruct *text,
		       int32 start_pos,
		       int32 end_pos,
		       XP_Bool need_bg)
{
  printf ("GNOME_DisplaySubtext\n");
  XP_ASSERT(0);
}

static void 
GNOMEFE_DisplayText (MWContext * context,
		    int iLocation,
		    LO_TextStruct *text,
		    XP_Bool need_bg)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return;

  moz_html_view_display_text(view, text, need_bg);
}

static void 
GNOMEFE_DisplayEmbed (MWContext * context,
		     int iLocation,
		     LO_EmbedStruct *embed_struct)
{
  XP_ASSERT(0);
  printf("GNOMEFE_DisplayEmbed (empty)\n");
}


static void 
GNOMEFE_DisplayJavaApp (MWContext * context,
		       int iLocation,
		       LO_JavaAppStruct *java_struct)
{
  XP_ASSERT(0);
  printf("GNOMEFE_DisplayJavaApp (empty)\n");
}

static void 
GNOMEFE_DisplayEdge (MWContext * context,
		    int iLocation,
		    LO_EdgeStruct *edge_struct)
{
  XP_ASSERT(0);
  printf("GNOMEFE_DisplayEdge (empty)\n");
}

static void 
GNOMEFE_DisplayTable (MWContext * context,
                      int iLocation,
                      LO_TableStruct *table_struct)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return;

  moz_html_view_display_table(view, table_struct);
}

static void 
GNOMEFE_DisplayCell (MWContext * context,
		    int iLocation,
		    LO_CellStruct *cell_struct)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return;

  moz_html_view_display_cell(view, cell_struct);
}

static void 
GNOMEFE_DisplaySubDoc (MWContext * context,
		      int iLocation,
		      LO_SubDocStruct *subdoc_struct)
{
  printf ("GNOME_DisplaySubdoc\n");
  XP_ASSERT(0);
}

static void 
GNOMEFE_DisplayLineFeed (MWContext * context,
                         int iLocation ,
                         LO_LinefeedStruct *line_feed,
                         XP_Bool need_bg)
{
  /* this should only do something if the context is an editor and the user has
     turned on display of paragraph marks. */
  MozEditorView *view = find_editor_view(context);

  if (!view) return;

  moz_editor_view_display_linefeed(view, line_feed, need_bg);
}

static void 
GNOMEFE_DisplayHR (MWContext * context,
                   int iLocation ,
                   LO_HorizRuleStruct *HR_struct)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return;

  moz_html_view_display_hr(view, HR_struct);
}

static void 
GNOMEFE_DisplayBullet (MWContext *context,
                       int iLocation,
                       LO_BullettStruct *bullet)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return;

  moz_html_view_display_bullet(view, bullet);
}

static void 
GNOMEFE_DisplayBorder (MWContext *context,
		      int iLocation,
		      int x,
		      int y,
		      int width,
		      int height,
		      int bw,
		      LO_Color *color,
		      LO_LineStyle style)
{
  printf ("GNOME_DisplayBorder\n");
  XP_ASSERT(0);
}

static void 
GNOMEFE_DisplayFeedback (MWContext *context,
			int iLocation,
			LO_Element *element)
{
  printf("GNOMEFE_DisplayFeedback (empty)\n");
}

static void 
GNOMEFE_ClearView (MWContext * context,
		  int which)
{
  printf ("GNOME_ClearView\n");
  XP_ASSERT(0);
}

static void 
GNOMEFE_SetDocDimension (MWContext *context,
			int iLocation,
			int32 iWidth,
			int32 iLength)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return; /* XXX */

  moz_html_view_set_doc_dimension(view, iWidth, iLength);
}

static void 
GNOMEFE_SetDocPosition (MWContext *context,
		       int iLocation,
		       int32 iX,
		       int32 iY)
{
  MozHTMLView *view = find_html_view(context);
  
  if (!view) return; /* XXX */

  printf("GNOMEFE_SetDocPosition: X %d, y %d\n", iX, iY);
  
  moz_html_view_set_doc_position(view, iX, iY);
}

static void 
GNOMEFE_GetDocPosition (MWContext *context,
		       int iLocation,
		       int32 *iX,
		       int32 *iY)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return; /* XXX */
  printf ("GNOME_GetDocPosition (old x %d, old y %d)\n", *iX, *iY);

  *iX = (int32) (view->doc_x);
  *iY = (int32) (view->doc_y);
  printf ("GNOME_GetDocPosition (x %d, y %d)\n", *iX, *iY);

}

static void 
GNOMEFE_BeginPreSection (MWContext *context)
{
  XP_ASSERT(0);
  printf("GNOMEFE_BeginPreSection (empty)\n");
}

static void 
GNOMEFE_EndPreSection (MWContext *context)
{
  XP_ASSERT(0);
  printf("GNOMEFE_EndPreSection (empty)\n");
}

static void
GNOMEFE_Progress(MWContext *context,
                 const char *msg)
{
  MozFrame *frame = find_frame(context);

  if (!frame) return; /* XXX */

  moz_frame_set_status(frame, (char*)msg);
}

static void 
GNOMEFE_SetProgressBarPercent(MWContext *context,
			      int32 percent)
{
  MozFrame *frame = find_frame(context);

  //  printf ("setprogressbarpercent %d\n", percent);

  if (!frame) return; /* XXX */

  if (percent == -1) percent = 0; /* XXXX */

  moz_frame_set_percent(frame, percent);
}

static void 
GNOMEFE_SetBackgroundColor (MWContext *context,
			   uint8 red,
			   uint8 green,
			   uint8 blue)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return; /* XXX */

  moz_html_view_set_background_color(view, red, green, blue);
}

static void 
GNOMEFE_SetCallNetlibAllTheTime (MWContext * win_id)
{
  printf ("set call netlib all the time\n");
  XP_ASSERT(0);
}

static void 
GNOMEFE_ClearCallNetlibAllTheTime (MWContext * win_id)
{
  printf ("clear call netlib all the time\n");
  XP_ASSERT(0);
}

static void 
GNOMEFE_GraphProgressInit (MWContext *context,
                           URL_Struct *URL_s,
                           int32 content_length)
{
  XP_ASSERT(0);
  printf("GNOMEFE_GraphProgressInit (empty)\n");
}

static void 
GNOMEFE_GraphProgressDestroy (MWContext *context,
                              URL_Struct *URL_s,
                              int32 content_length,
                              int32 total_bytes_read)
{
  XP_ASSERT(0);
  printf("GNOMEFE_GraphProgressDestroy (empty)\n");
}

static void 
GNOMEFE_GraphProgress (MWContext *context,
                       URL_Struct *URL_s,
                       int32 bytes_received,
                       int32 bytes_since_last_time,
                       int32 content_length)
{
  XP_ASSERT(0);
  printf("GNOMEFE_GraphProgress (empty)\n");
}

static XP_Bool 
GNOMEFE_UseFancyFTP (MWContext * window_id)
{
  return TRUE;
}

static XP_Bool 
GNOMEFE_UseFancyNewsgroupListing (MWContext *window_id)
{
  return FALSE;
}

static int 
GNOMEFE_FileSortMethod (MWContext * window_id)
{
  return -1;
}

static XP_Bool 
GNOMEFE_ShowAllNewsArticles (MWContext *window_id)
{
  return FALSE;
}

static void
GNOMEFE_Alert(MWContext *context,
	     const char *msg)
{
  printf ("uh oh...\n");
  XP_ASSERT(0);
}

static int done = 0;
static XP_Bool ret_val = FALSE;

static void
ok_cancel_callback(GtkWidget *widget,
                   int button,
                   void *data)
{
  ret_val = (button == 0) /* the ok button is 0, cancel is 1 */;
  done = 1;
}

/*
** FE_Confirm - Put up an confirmation dialog (with Yes and No
** buttons) and return TRUE if Yes/Ok was clicked and FALSE if
** No/Cancel was clicked.
**
** This method should not return until the user has clicked on one of
** the buttons.
*/
static XP_Bool 
GNOMEFE_Confirm(MWContext * context,
	       const char * Msg)
{
  GtkWidget *dialog = gnome_message_box_new(Msg,
                                            GNOME_MESSAGE_BOX_QUESTION,
                                            GNOME_STOCK_BUTTON_OK,
                                            GNOME_STOCK_BUTTON_CANCEL,
                                            NULL);

  gtk_signal_connect(GTK_OBJECT(dialog),
                     "clicked",
                     (GtkSignalFunc)ok_cancel_callback, NULL);
                     

  gnome_dialog_set_modal(GNOME_DIALOG(dialog));
  gnome_dialog_set_default(GNOME_DIALOG(dialog), 1);

  gtk_window_set_policy(GTK_WINDOW(dialog),
                        TRUE,
                        TRUE,
                        FALSE);

  gtk_widget_show(dialog);

  done = 0;

  while (!done)
    gtk_main_iteration_do(TRUE);

  return ret_val;
}

static XP_Bool
GNOMEFE_CheckConfirm(MWContext *pContext,
                     const char *pConfirmMessage,
                     const char *pCheckMessage,
                     const char *pOKMessage,
                     const char *pCancelMessage,
                     XP_Bool *pChecked)
{
 return FALSE;
 XP_ASSERT(0);
}

static XP_Bool
GNOMEFE_SelectDialog(MWContext *pContext, const char *pMessage, const char **pList, int16 *pCount)
{
  return FALSE;
  XP_ASSERT(0);
}

static char* 
GNOMEFE_Prompt(MWContext * context,
	      const char * Msg,
	      const char * dflt)
{
  return NULL;
  XP_ASSERT(0);
}

static char* 
GNOMEFE_PromptWithCaption(MWContext * context,
			 const char *caption,
			 const char * Msg,
			 const char * dflt)
{
  return NULL;
  XP_ASSERT(0);
}

static XP_Bool 
GNOMEFE_PromptUsernameAndPassword (MWContext *context,
				  const char * message,
				  char **username,
				  char **password)
{
  return FALSE;
  XP_ASSERT(0);
}

static char* 
GNOMEFE_PromptPassword(MWContext * context,
		      const char * Msg)
{
  return NULL;
  XP_ASSERT(0);
}

static void 
GNOMEFE_EnableClicking(MWContext* context)
{
  printf("GNOMEFE_EnableClicking (empty)\n");
}

static void 
GNOMEFE_AllConnectionsComplete(MWContext * context)
{
  printf("GNOMEFE_AllConnectionsComplete (empty)\n");
}

static void 
GNOMEFE_EraseBackground (MWContext * context,
                         int iLocation,
                         int32 x,
                         int32 y,
                         uint32 width,
                         uint32 height,
                         LO_Color *bg)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return;
  printf("GNOMEFE_EraseBackground\n");

  moz_html_view_erase_background(view, x, y, width, height, bg);
}

void 
GNOMEFE_SetDrawable (MWContext *context,
                     CL_Drawable *drawable)
{
  MozHTMLView *view = find_html_view(context);

  if (!view) return;

  moz_html_view_set_drawable(view, drawable);
}

static void 
GNOMEFE_GetTextFrame (MWContext *context,
                      LO_TextStruct *text,
                      int32 start,
                      int32 end,
                      XP_Rect *frame)
{
  XP_ASSERT(0);
  printf("GNOMEFE_GetTextFrame (empty)\n");
}

/* these functions are to allow dealyed native window applet creation and transparent applet */
static void 
GNOMEFE_HandleClippingView (MWContext *pContext,
			   struct LJAppletData *appletD,
			   int x,
			   int y,
			   int width,
			   int height)
{
  XP_ASSERT(0);
  printf("GNOMEFE_HandleClippingView (empty)\n");
}

static void 
GNOMEFE_DrawJavaApp (MWContext *pContext,
		    int iLocation,
		    LO_JavaAppStruct *pJava)
{
  XP_ASSERT(0);
  printf("GNOMEFE_DrawJavaApp (empty)\n");
}

static void
GNOMEFE_DisplayBuiltin (MWContext *context, int iLocation,
                        LO_BuiltinStruct *builtin_struct)
{
  XP_ASSERT(0);
  printf("GNOMEFE_DisplayBuiltin (empty)\n");
}

static void
GNOMEFE_FreeBuiltinElement(MWContext *pContext, LO_BuiltinStruct *builtin_struct)
{
  GtkWidget *view;

  if (!builtin_struct || !builtin_struct->FE_Data) return;

  view = (GtkWidget *)builtin_struct->FE_Data;

  gtk_widget_destroy(view);

  builtin_struct->FE_Data = NULL;
}

static ContextFuncs _gnomefe_funcs = {
#define FE_DEFINE(func, returns, args) GNOMEFE##_##func,
#include "mk_cx_fn.h"
};

/* Given a mask value, returns the index of the first set bit, and the
   number of consecutive bits set. */
static void
FindShift (unsigned long mask, uint8 *shiftp, uint8* bitsp)
{
  uint8 shift = 0;
  uint8 bits = 0;
  while ((mask & 1) == 0) {
    shift++;
    mask >>= 1;
  }
  while ((mask & 1) == 1) {
    bits++;
    mask >>= 1;
  }
  *shiftp = shift;
  *bitsp = bits;
}


MWContext*
GNOMEFE_CreateMWContext()
{
  MWContext *context = XP_NewContext();
  IL_RGBBits rgb;
  GdkVisual *visual = gdk_visual_get_best();
  
  XP_ASSERT(context);
  if (!context) return NULL;

  context->funcs = &_gnomefe_funcs;

  context->fe.data = (fe_ContextData *) calloc(1, sizeof(fe_ContextData));

  context->convertPixX = context->convertPixY = 1;

  memset(&rgb, '\0', sizeof(rgb));

  FindShift (visual->red_mask, &rgb.red_shift, &rgb.red_bits);
  FindShift (visual->green_mask, &rgb.green_shift, &rgb.green_bits);
  FindShift (visual->blue_mask, &rgb.blue_shift, &rgb.blue_bits);

  context->color_space = IL_CreateTrueColorSpace(&rgb,
                                                 gdk_visual_get_best_depth());

  return context;
}

int32
FE_GetContextID(MWContext *window_id)
{
  return (int32)window_id; /* XXX not 64 bit clean */
}

static MWContext *init_context;
MWContext*
FE_GetInitContext()
{
  printf ("FE_GetInitContext()\n");

  if (!init_context) init_context = GNOMEFE_CreateMWContext();

  return init_context;
}

MWContext*
FE_MakeNewWindow(MWContext *old_context,
		 URL_Struct *url,
		 char *window_name,
		 Chrome *chrome)
{
  printf ("FE_MakeNewWindow\n");
  XP_ASSERT(0);
  return NULL;
}

void
FE_DestroyWindow(MWContext *context)
{
  printf ("FE_DestroyWindow()\n");
  XP_ASSERT(0);
}

void
FE_UpdateStopState(MWContext *context)
{
  printf ("FE_UpdateStopState()\n");
  XP_ASSERT(0);
}

void
FE_UpdateChrome(MWContext *window,
		Chrome *chrome)
{
  printf ("FE_UpdateChrome()\n");
  XP_ASSERT(0);
}

void
FE_QueryChrome(MWContext *window,
	       Chrome *chrome)
{
  printf ("FE_QueryChrome()\n");
  XP_ASSERT(0);
}

void 
FE_BackCommand(MWContext *context)
{
  printf ("FE_BackCommand()\n");
  XP_ASSERT(0);
}

void 
FE_ForwardCommand(MWContext *context)
{
  printf ("FE_ForwardCommand()\n");
  XP_ASSERT(0);
}

void 
FE_HomeCommand(MWContext *context)
{
  printf ("FE_HomeCommand()\n");
  XP_ASSERT(0);
}

void 
FE_PrintCommand(MWContext *context)
{
  printf ("FE_PrintCommand()\n");
  XP_ASSERT(0);
}

XP_Bool
FE_FindCommand(MWContext *context,
	       char *text,
	       XP_Bool case_sensitive,
	       XP_Bool backwards,
	       XP_Bool wrap)
{
  printf ("FE_FindCommand()\n");
  XP_ASSERT(0);
}

void 
FE_GetWindowOffset(MWContext *pContext, 
		   int32 *sx, 
		   int32 *sy)
{
  MozHTMLView *view = find_html_view(pContext);

  gdk_window_get_origin(MOZ_COMPONENT(view)->base_widget->window,
                        sx, sy);
}

void 
FE_GetScreenSize(MWContext *pContext, 
		 int32 *sx, 
		 int32 *sy)
{
  printf ("FE_GetScreenSize\n");
  XP_ASSERT(0);
}

void 
FE_GetAvailScreenRect(MWContext *pContext, 
		      int32 *sx, 
		      int32 *sy, 
		      int32 *left, 
		      int32 *top)
{
  printf ("FE_GetAvailScreenRect\n");
  XP_ASSERT(0);
}

void 
FE_GetPixelAndColorDepth(MWContext *pContext, 
			 int32 *pixelDepth, 
			 int32 *colorDepth)
{
  printf ("FE_GetPixelAndColorDepth\n");
  XP_ASSERT(0);
}

void
FE_ShiftImage (MWContext *context,
	       LO_ImageStruct *lo_image)
{
  printf ("FE_ShiftImage()\n");
  XP_ASSERT(0);
}

void
FE_ScrollDocTo (MWContext *context,
		int iLocation,
		int32 x,
		int32 y)
{
  printf ("FE_ScrollDocTo\n");
  XP_ASSERT(0);
}

void
FE_ScrollDocBy (MWContext *context,
		int iLocation,
		int32 x,
		int32 y)
{
  printf ("FE_ScrollDocBy\n");
  XP_ASSERT(0);
}

int
FE_GetURL (MWContext *context,
	   URL_Struct *url)
{
  printf ("FE_GetURL()\n");
  XP_ASSERT(0);
}

void
FE_SetRefreshURLTimer(MWContext *context, 
		      URL_Struct *URL_s)
{
  printf ("FE_SetRefreshURLTimer\n");
  XP_ASSERT(0);
}

int
FE_EnableBackButton(MWContext* context)
{
  printf ("FE_EnableBackButton()\n");
  XP_ASSERT(0);
}

int
FE_EnableForwardButton(MWContext* context)
{
  printf ("FE_EnableForwardButton()\n");
  XP_ASSERT(0);
}

int
FE_DisableBackButton(MWContext* context)
{
  printf ("FE_DisableBackButton()\n");
  XP_ASSERT(0);
}

int
FE_DisableForwardButton(MWContext* context)
{
  printf ("FE_DisableForwardButton()\n");
  XP_ASSERT(0);
}

MWContext *
FE_MakeBlankWindow(MWContext *old_context,
		   URL_Struct *url,
		   char *window_name)
{
  printf ("FE_MakeBlankWindow()\n");
  XP_ASSERT(0);
}

void
FE_SetWindowLoading(MWContext *context,
		    URL_Struct *url,
		    Net_GetUrlExitFunc **exit_func)
{
  printf ("FE_SetWindowLoading()\n");
  XP_ASSERT(0);
}

void
FE_RaiseWindow(MWContext *context)
{
  printf ("FE_RaiseWindow()\n");
  XP_ASSERT(0);
}

void
FE_ConnectToRemoteHost(MWContext* ctxt,
		       int url_type,
		       char* hostname,
		       char* port,
		       char* username)
{
  XP_ASSERT(0);
  printf("FE_ConnectToRemoteHost (empty)\n");
}

void*
FE_AboutData(const char* which,
	     char** data_ret,
	     int32* length_ret,
	     char** content_type_ret)
{
  unsigned char *tmp;
  static XP_Bool ever_loaded_map = FALSE;
  char *rv = NULL;

  printf ("FE_AboutData\n");
  if (0) {;} 
#ifndef NETSCAPE_PRIV
  else if (!strcmp (which, "flamer"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) flamer_gif;
      *length_ret = sizeof (flamer_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#else
  else if (!strcmp (which, "logo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) biglogo_gif;
      *length_ret = sizeof (biglogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "photo"))
    {
      if (!ever_loaded_map)
	{
	  *data_ret = 0;
	  *length_ret = 0;
	  *content_type_ret = 0;
	  return 0;
	}
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) photo_jpg;
      *length_ret = sizeof (photo_jpg) - 1;
      *content_type_ret = IMAGE_JPG;
    }
  else if (!strcmp (which, "hype"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char*)hype_au;
      *length_ret = sizeof (hype_au) - 1;
      *content_type_ret = AUDIO_BASIC;
    }
#ifdef MOZ_SECURITY
  else if (!strcmp (which, "rsalogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) rsalogo_gif;
      *length_ret = sizeof (rsalogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif
#ifdef JAVA
  else if (!strcmp (which, "javalogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) javalogo_gif;
      *length_ret = sizeof (javalogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif
#ifdef HAVE_QUICKTIME
  else if (!strcmp (which, "qtlogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) qt_logo_gif;
      *length_ret = sizeof (qt_logo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif
#ifdef FORTEZZA
  else if (!strcmp (which, "litronic"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) litronic_gif;
      *length_ret = sizeof (litronic_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif
  else if (!strcmp (which, "coslogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) coslogo_jpg;
      *length_ret = sizeof (coslogo_jpg) - 1;
      *content_type_ret = IMAGE_JPG;
    }
  else if (!strcmp (which, "insologo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) insologo_gif;
      *length_ret = sizeof (insologo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "mclogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) mclogo_gif;
      *length_ret = sizeof (mclogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "ncclogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) ncclogo_gif;
      *length_ret = sizeof (ncclogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "odilogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) odilogo_gif;
      *length_ret = sizeof (odilogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "tdlogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) tdlogo_gif;
      *length_ret = sizeof (tdlogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
  else if (!strcmp (which, "visilogo"))
    {
      /* Note, this one returns a read-only string. */
      *data_ret = (char *) visilogo_gif;
      *length_ret = sizeof (visilogo_gif) - 1;
      *content_type_ret = IMAGE_GIF;
    }
#endif /* !NETSCAPE_PRIV */
#ifdef EDITOR
  else if (!strcmp(which, "editfilenew"))
    {
      /* Magic about: for Editor new (blank) window */
      *data_ret = strdup(EDT_GetEmptyDocumentString());
      *length_ret = strlen (*data_ret);
      *content_type_ret = TEXT_MDL; 
    }
#endif
  else
    {
	  char *a = NULL;
      char *type = TEXT_HTML;
      Bool do_PR_snprintf = FALSE;
      Bool do_rot = TRUE;
      if (!strcmp (which, ""))
	{
#if 0
	  do_PR_snprintf = TRUE;
	  a = fe_get_localized_file(XFE_ABOUT_FILE);
	  if (a)
	    {
	      a = strdup(a);
	      do_rot = False;
	    }
	  else
#endif
	    {
	      a = strdup (
#ifdef JAVA
#ifndef MOZ_COMMUNICATOR_ABOUT
#		          include "xp/about-java-lite.h"
#else
#		          include "xp/about-java.h"
#endif
#else
#ifndef MOZ_COMMUNICATOR_ABOUT
#		          include "xp/about-lite.h"
#else
#		          include "xp/about.h"
#endif
#endif
		         );
	    }
	}
      else if (!strcmp (which, "splash"))
	{
	  do_PR_snprintf = TRUE;
#if 0
	  a = fe_get_localized_file(XFE_SPLASH_FILE);
	  if (a)
	    {
	      a = strdup(a);
	      do_rot = False;
	    }
	  else
#endif
	    {
	      a = strdup (
#ifdef JAVA
#ifndef MOZ_COMMUNICATOR_ABOUT
#		          include "xp/splash-java-lite.h"
#else
#		          include "xp/splash-java.h"
#endif
#else
#ifndef MOZ_COMMUNICATOR_ABOUT
#		          include "xp/splash-lite.h"
#else
#		          include "xp/splash.h"
#endif
#endif
		         );
	    }
	}
      else if (!strcmp (which, "1994"))
	{
	  ever_loaded_map = TRUE;
	  a = strdup (
#		      include "xp/authors2.h"
		     );
	}
      else if (!strcmp (which,"license"))
	{
	  type = TEXT_PLAIN;
#if 0
	  a = fe_get_localized_file(XFE_LICENSE_FILE);
	  if (a)
	    {
	      a = strdup(a);
	      do_rot = False;
	    }
	  else
	    {
	      a = strdup (fe_LicenseData);
	    }
#endif
	}

      else if (!strcmp (which,"mozilla"))
	{
	  a = strdup (
#		      include "xp/mozilla.h"
		      );
	}
      else if (!strcmp (which,
			"mailintro"))
	{
	  type = MESSAGE_RFC822;
#if 0
     a = fe_get_localized_file(XFE_MAILINTRO_FILE );
	  if (a)
	  {
        a = strdup( a );
        do_rot = False;
     }
     else
#endif
     {
		  a = strdup (
#		      include "xp/mail.h"
		      );
     }
	}

      else if (!strcmp (which, "blank"))
	a = strdup ("");
#if 0
      else if (!strcmp (which, "custom"))
        {
          do_rot = False;
	  a = ekit_AboutData();
        }
#endif

      else if (!strcmp (which, "plugins"))
	{
#if 0
	  a = fe_get_localized_file(XFE_PLUGIN_FILE);
	  if (a)
	    {
	      a = strdup(a);
	      do_rot = False;
	    }
	  else
#endif
	    {
	      a = strdup (
#		          include "xp/aboutplg.h"
		         );
	    }
	}



      else
	a = strdup ("\234\255\246\271\250\255\252\274\145\271\246"
		    "\261\260\256\263\154\145\154\247\264\272\271"
		    "\161\145\234\256\261\261\256\270\204");
      if (a)
	{
	  if (do_rot)
	    {
	      for (tmp = (unsigned char *) a; *tmp; tmp++) *tmp -= 69;
	    }

#if 0
	  if (do_PR_snprintf)
	    {
	      char *a2;
	      int len;
#ifdef MOZ_SECURITY
	      char *s0 = XP_GetString(XFE_SECURITY_WITH);
	      char *s1 = XP_SecurityVersion(1);
	      char *s2 = XP_SecurityCapabilities();
	      char *ss;
	      len = strlen(s0)+strlen(s1)+strlen(s2);
	      ss = (char*) malloc(len);
	      PR_snprintf(ss, len, s0, s1, s2);
#else
	      char *ss = XP_GetString(XFE_SECURITY_DISABLED);
#endif
	      len = strlen(a) + strlen(fe_version_and_locale) +
                        strlen(fe_version_and_locale) + strlen(ss);
	      a2 = (char *) malloc(len);
	      PR_snprintf (a2, len, a,
		       fe_version_and_locale,
		       fe_version_and_locale,
		       ss
		       );
#ifdef MOZ_SECURITY
	      free (s2);
	      free (ss);
#endif
	      free (a);
	      a = a2;
	    }

#endif
	  *data_ret = a;
	  rv = a;  /* Return means 'free this later' */
	  *length_ret = strlen (*data_ret);
	  *content_type_ret = type;
	}
      else
	{
	  *data_ret = 0;
	  *length_ret = 0;
	  *content_type_ret = 0;
	}
    }
  return rv;
}

void
FE_FreeAboutData(void* data,
		 const char* which)
{
  printf ("FE_FreeAboutData\n");
  if (data) free(data);
}

XP_Bool
FE_IsNetcasterInstalled()
{
  return FALSE;
}

int FE_AskStreamQuestion (MWContext *window_id) {
	return 1;
}
