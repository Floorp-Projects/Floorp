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
   stubctxt.c --- stub fe handling of MWContext initialization.
*/

#include "structs.h"
#include "ntypes.h"
#include "xpassert.h"
#include "proto.h"
#include "fe_proto.h"

#include "stubform.h"

static MWContext*
STUBFE_CreateNewDocWindow(MWContext *calling_context,
			  URL_Struct *URL)
{
  return NULL;
}

static void
STUBFE_LayoutNewDocument(MWContext *context,
			 URL_Struct *url_struct,
			 int32 *iWidth,
			 int32 *iHeight,
			 int32 *mWidth,
			 int32 *mHeight)
{
}

static void 
STUBFE_SetDocTitle (MWContext * context,
		    char * title)
{
}

static void 
STUBFE_FinishedLayout (MWContext *context)
{
}

static char* 
STUBFE_TranslateISOText (MWContext * context,
			 int charset,
			 char *ISO_Text)
{
  return NULL;
}

static int 
STUBFE_GetTextInfo (MWContext * context,
		    LO_TextStruct *text,
		    LO_TextInfo *text_info)
{
  return -1;
}

static void 
STUBFE_GetEmbedSize (MWContext * context,
		     LO_EmbedStruct *embed_struct,
		     NET_ReloadMethod force_reload)
{
}

static void 
STUBFE_GetJavaAppSize (MWContext * context,
		       LO_JavaAppStruct *java_struct,
		       NET_ReloadMethod force_reload)
{
}

static void 
STUBFE_FreeEmbedElement (MWContext *context,
			 LO_EmbedStruct *embed)
{
}

static void 
STUBFE_FreeJavaAppElement (MWContext *context,
			   struct LJAppletData *appletData)
{
}

static void 
STUBFE_HideJavaAppElement (MWContext *context,
			   struct LJAppletData *java_app)
{
}

static void 
STUBFE_FreeEdgeElement (MWContext *context,
			LO_EdgeStruct *edge)
{
}

static void 
STUBFE_FormTextIsSubmit (MWContext * context,
			 LO_FormElementStruct * form_element)
{
}

static void 
STUBFE_DisplaySubtext (MWContext * context,
		       int iLocation,
		       LO_TextStruct *text,
		       int32 start_pos,
		       int32 end_pos,
		       XP_Bool need_bg)
{
}

static void 
STUBFE_DisplayText (MWContext * context,
		    int iLocation,
		    LO_TextStruct *text,
		    XP_Bool need_bg)
{
}

static void 
STUBFE_DisplayEmbed (MWContext * context,
		     int iLocation,
		     LO_EmbedStruct *embed_struct)
{
}


static void 
STUBFE_DisplayJavaApp (MWContext * context,
		       int iLocation,
		       LO_JavaAppStruct *java_struct)
{
}

static void 
STUBFE_DisplayEdge (MWContext * context,
		    int iLocation,
		    LO_EdgeStruct *edge_struct)
{
}

static void 
STUBFE_DisplayTable (MWContext * context,
		     int iLocation,
		     LO_TableStruct *table_struct)
{
}

static void 
STUBFE_DisplayCell (MWContext * context,
		    int iLocation,
		    LO_CellStruct *cell_struct)
{
}

static void 
STUBFE_DisplaySubDoc (MWContext * context,
		      int iLocation,
		      LO_SubDocStruct *subdoc_struct)
{
}

static void 
STUBFE_DisplayLineFeed (MWContext * context,
			int iLocation ,
			LO_LinefeedStruct *line_feed,
			XP_Bool need_bg)
{
}

static void 
STUBFE_DisplayHR (MWContext * context,
		  int iLocation ,
		  LO_HorizRuleStruct *HR_struct)
{
}

static void 
STUBFE_DisplayBullet (MWContext *context,
		      int iLocation,
		      LO_BullettStruct *bullet)
{
}

static void 
STUBFE_DisplayFormElement (MWContext * context,
			   int iLocation,
			   LO_FormElementStruct * form_element)
{
}

static void 
STUBFE_DisplayBorder (MWContext *context,
		      int iLocation,
		      int x,
		      int y,
		      int width,
		      int height,
		      int bw,
		      LO_Color *color,
		      LO_LineStyle style)
{
}

static void 
STUBFE_DisplayFeedback (MWContext *context,
			int iLocation,
			LO_Element *element)
{
}

static void 
STUBFE_ClearView (MWContext * context,
		  int which)
{
}

static void 
STUBFE_SetDocDimension (MWContext *context,
			int iLocation,
			int32 iWidth,
			int32 iLength)
{
}

static void 
STUBFE_SetDocPosition (MWContext *context,
		       int iLocation,
		       int32 iX,
		       int32 iY)
{
}

static void 
STUBFE_GetDocPosition (MWContext *context,
		       int iLocation,
		       int32 *iX,
		       int32 *iY)
{
}

static void 
STUBFE_BeginPreSection (MWContext *context)
{
}

static void 
STUBFE_EndPreSection (MWContext *context)
{
}

static void
STUBFE_Progress(MWContext *context,
		const char *msg)
{
}

static void 
STUBFE_SetProgressBarPercent (MWContext *context,
			      int32 percent)
{
}

static void 
STUBFE_SetBackgroundColor (MWContext *context,
			   uint8 red,
			   uint8 green,
			   uint8 blue)
{
}

static void 
STUBFE_SetCallNetlibAllTheTime (MWContext * win_id)
{
}

static void 
STUBFE_ClearCallNetlibAllTheTime (MWContext * win_id)
{
}

static void 
STUBFE_GraphProgressInit (MWContext *context,
			  URL_Struct *URL_s,
			  int32 content_length)
{
}

static void 
STUBFE_GraphProgressDestroy (MWContext *context,
			     URL_Struct *URL_s,
			     int32 content_length,
			     int32 total_bytes_read)
{
}

static void 
STUBFE_GraphProgress (MWContext *context,
		      URL_Struct *URL_s,
		      int32 bytes_received,
		      int32 bytes_since_last_time,
		      int32 content_length)
{
}

static XP_Bool 
STUBFE_UseFancyFTP (MWContext * window_id)
{
  return FALSE;
}

static XP_Bool 
STUBFE_UseFancyNewsgroupListing (MWContext *window_id)
{
  return FALSE;
}

static int 
STUBFE_FileSortMethod (MWContext * window_id)
{
  return -1;
}

static XP_Bool 
STUBFE_ShowAllNewsArticles (MWContext *window_id)
{
  return FALSE;
}

static void
STUBFE_Alert(MWContext *context,
	     const char *msg)
{
}

static XP_Bool 
STUBFE_Confirm(MWContext * context,
	       const char * Msg)
{
  return FALSE;
}

static char* 
STUBFE_Prompt(MWContext * context,
	      const char * Msg,
	      const char * dflt)
{
  return NULL;
}

static char* 
STUBFE_PromptWithCaption(MWContext * context,
			 const char *caption,
			 const char * Msg,
			 const char * dflt)
{
  return NULL;
}

static XP_Bool 
STUBFE_PromptUsernameAndPassword (MWContext *context,
				  const char * message,
				  char **username,
				  char **password)
{
  return FALSE;
}

static char* 
STUBFE_PromptPassword(MWContext * context,
		      const char * Msg)
{
  return NULL;
}

static void 
STUBFE_EnableClicking(MWContext* context)
{
}

static void 
STUBFE_AllConnectionsComplete(MWContext * context)
{
}

static void 
STUBFE_EraseBackground (MWContext * context,
			int iLocation,
			int32 x,
			int32 y,
			uint32 width,
			uint32 height,
			LO_Color *bg)
{
}

static void 
STUBFE_SetDrawable (MWContext *context,
		    CL_Drawable *drawable)
{
}

static void 
STUBFE_GetTextFrame (MWContext *context,
		     LO_TextStruct *text,
		     int32 start,
		     int32 end,
		     XP_Rect *frame)
{
}

/* these functions are to allow dealyed native window applet creation and transparent applet */
static void 
STUBFE_HandleClippingView (MWContext *pContext,
			   struct LJAppletData *appletD,
			   int x,
			   int y,
			   int width,
			   int height)
{
}

static void 
STUBFE_DrawJavaApp (MWContext *pContext,
		    int iLocation,
		    LO_JavaAppStruct *pJava)
{
}

static ContextFuncs _stubfe_funcs = {
#define FE_DEFINE(func, returns, args) STUBFE##_##func,
#include "mk_cx_fn.h"
};

MWContext*
STUBFE_CreateMWContext()
{
  MWContext *context = XP_NewContext();

  XP_ASSERT(context);
  if (!context) return NULL;

  context->funcs = &_stubfe_funcs;

  return context;
}

int32
FE_GetContextID(MWContext *window_id)
{
}

MWContext*
FE_GetInitContext()
{
}

MWContext*
FE_MakeNewWindow(MWContext *old_context,
		 URL_Struct *url,
		 char *window_name,
		 Chrome *chrome)
{
}

void
FE_DestroyWindow(MWContext *context)
{
}

void
FE_UpdateStopState(MWContext *context)
{
}

void
FE_UpdateChrome(MWContext *window,
		Chrome *chrome)
{
}

void
FE_QueryChrome(MWContext *window,
	       Chrome *chrome)
{
}

void 
FE_BackCommand(MWContext *context)
{
}

void 
FE_ForwardCommand(MWContext *context)
{
}

void 
FE_HomeCommand(MWContext *context)
{
}

void 
FE_PrintCommand(MWContext *context)
{
}

XP_Bool
FE_FindCommand(MWContext *context,
	       char *text,
	       XP_Bool case_sensitive,
	       XP_Bool backwards,
	       XP_Bool wrap)
{
}

void 
FE_GetWindowOffset(MWContext *pContext, 
		   int32 *sx, 
		   int32 *sy)
{
}

void 
FE_GetScreenSize(MWContext *pContext, 
		 int32 *sx, 
		 int32 *sy)
{
}

void 
FE_GetAvailScreenRect(MWContext *pContext, 
		      int32 *sx, 
		      int32 *sy, 
		      int32 *left, 
		      int32 *top)
{
}

void 
FE_GetPixelAndColorDepth(MWContext *pContext, 
			 int32 *pixelDepth, 
			 int32 *colorDepth)
{
}

void
FE_ShiftImage (MWContext *context,
	       LO_ImageStruct *lo_image)
{
}

void
FE_ScrollDocTo (MWContext *context,
		int iLocation,
		int32 x,
		int32 y)
{
}

void
FE_ScrollDocBy (MWContext *context,
		int iLocation,
		int32 x,
		int32 y)
{
}

int
FE_GetURL (MWContext *context,
	   URL_Struct *url)
{
}

void
FE_SetRefreshURLTimer(MWContext *context, 
		      uint32     seconds, 
		      char      *refresh_url)
{
}

int
FE_EnableBackButton(MWContext* context)
{
}

int
FE_EnableForwardButton(MWContext* context)
{
}

int
FE_DisableBackButton(MWContext* context)
{
}

int
FE_DisableForwardButton(MWContext* context)
{
}

MWContext *
FE_MakeBlankWindow(MWContext *old_context,
		   URL_Struct *url,
		   char *window_name)
{
}

void
FE_SetWindowLoading(MWContext *context,
		    URL_Struct *url,
		    Net_GetUrlExitFunc **exit_func)
{
}

void
FE_RaiseWindow(MWContext *context)
{
}

void
FE_ConnectToRemoteHost(MWContext* ctxt,
		       int url_type,
		       char* hostname,
		       char* port,
		       char* username)
{
}

void*
FE_AboutData(const char* which,
	     char** data_ret,
	     int32* length_ret,
	     char** content_type_ret)
{
}

void
FE_FreeAboutData(void* data,
		 const char* which)
{
}

XP_Bool
FE_IsNetcasterInstalled()
{
}
