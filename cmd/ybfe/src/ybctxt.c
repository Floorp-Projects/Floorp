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
   ybctxt.c --- yb fe handling of MWContext initialization.
*/

#include "structs.h"
#include "ntypes.h"
#include "xpassert.h"
#include "proto.h"
#include "fe_proto.h"

#include "ybform.h"

static MWContext*
YBFE_CreateNewDocWindow(MWContext *calling_context,
			  URL_Struct *URL)
{
  return NULL;
}

static void
YBFE_LayoutNewDocument(MWContext *context,
			 URL_Struct *url_struct,
			 int32 *iWidth,
			 int32 *iHeight,
			 int32 *mWidth,
			 int32 *mHeight)
{
}

static void 
YBFE_SetDocTitle (MWContext * context,
		    char * title)
{
}

static void 
YBFE_FinishedLayout (MWContext *context)
{
}

static char* 
YBFE_TranslateISOText (MWContext * context,
			 int charset,
			 char *ISO_Text)
{
  return NULL;
}

static int 
YBFE_GetTextInfo (MWContext * context,
		    LO_TextStruct *text,
		    LO_TextInfo *text_info)
{
  return -1;
}

static void 
YBFE_GetEmbedSize (MWContext * context,
		     LO_EmbedStruct *embed_struct,
		     NET_ReloadMethod force_reload)
{
}

static void 
YBFE_GetJavaAppSize (MWContext * context,
		       LO_JavaAppStruct *java_struct,
		       NET_ReloadMethod force_reload)
{
}

static void 
YBFE_FreeEmbedElement (MWContext *context,
					   LO_EmbedStruct *embed)
{
}

static void 
YBFE_CreateEmbedWindow (MWContext *context,
						NPEmbeddedApp *app)
{
}

static void 
YBFE_SaveEmbedWindow (MWContext *context,
					  NPEmbeddedApp *app)
{
}

static void 
YBFE_RestoreEmbedWindow (MWContext *context,
						 NPEmbeddedApp *app)
{
}

static void 
YBFE_DestroyEmbedWindow (MWContext *context,
						 NPEmbeddedApp *app)
{
}

static void 
YBFE_FreeBuiltinElement(MWContext *context,
						LO_BuiltinStruct *embed)
{
}

static void 
YBFE_FreeJavaAppElement (MWContext *context,
			   struct LJAppletData *appletData)
{
}

static void 
YBFE_HideJavaAppElement (MWContext *context,
			   struct LJAppletData *java_app)
{
}

static void 
YBFE_FreeEdgeElement (MWContext *context,
			LO_EdgeStruct *edge)
{
}

static void 
YBFE_FormTextIsSubmit (MWContext * context,
			 LO_FormElementStruct * form_element)
{
}

static void 
YBFE_DisplaySubtext (MWContext * context,
		       int iLocation,
		       LO_TextStruct *text,
		       int32 start_pos,
		       int32 end_pos,
		       XP_Bool need_bg)
{
}

static void 
YBFE_DisplayText (MWContext * context,
		    int iLocation,
		    LO_TextStruct *text,
		    XP_Bool need_bg)
{
}

static void 
YBFE_DisplayEmbed (MWContext * context,
		     int iLocation,
		     LO_EmbedStruct *embed_struct)
{
}

static void 
YBFE_DisplayBuiltin (MWContext * context,
					   int iLocation,
					   LO_BuiltinStruct *builtin_struct)
{
}

static void 
YBFE_DisplayJavaApp (MWContext * context,
		       int iLocation,
		       LO_JavaAppStruct *java_struct)
{
}

static void 
YBFE_DisplayEdge (MWContext * context,
		    int iLocation,
		    LO_EdgeStruct *edge_struct)
{
}

static void 
YBFE_DisplayTable (MWContext * context,
		     int iLocation,
		     LO_TableStruct *table_struct)
{
}

static void 
YBFE_DisplayCell (MWContext * context,
		    int iLocation,
		    LO_CellStruct *cell_struct)
{
}

static void 
YBFE_DisplaySubDoc (MWContext * context,
		      int iLocation,
		      LO_SubDocStruct *subdoc_struct)
{
}

static void 
YBFE_DisplayLineFeed (MWContext * context,
			int iLocation ,
			LO_LinefeedStruct *line_feed,
			XP_Bool need_bg)
{
}

static void 
YBFE_DisplayHR (MWContext * context,
		  int iLocation ,
		  LO_HorizRuleStruct *HR_struct)
{
}

static void 
YBFE_DisplayBullet (MWContext *context,
		      int iLocation,
		      LO_BullettStruct *bullet)
{
}

static void 
YBFE_DisplayFormElement (MWContext * context,
			   int iLocation,
			   LO_FormElementStruct * form_element)
{
}

static void 
YBFE_DisplayBorder (MWContext *context,
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
YBFE_DisplayFeedback (MWContext *context,
			int iLocation,
			LO_Element *element)
{
}

static void 
YBFE_ClearView (MWContext * context,
		  int which)
{
}

static void 
YBFE_SetDocDimension (MWContext *context,
			int iLocation,
			int32 iWidth,
			int32 iLength)
{
}

static void 
YBFE_SetDocPosition (MWContext *context,
		       int iLocation,
		       int32 iX,
		       int32 iY)
{
}

static void 
YBFE_GetDocPosition (MWContext *context,
		       int iLocation,
		       int32 *iX,
		       int32 *iY)
{
}

static void 
YBFE_BeginPreSection (MWContext *context)
{
}

static void 
YBFE_EndPreSection (MWContext *context)
{
}

static void
YBFE_Progress(MWContext *context,
		const char *msg)
{
}

static void 
YBFE_SetProgressBarPercent (MWContext *context,
			      int32 percent)
{
}

static void 
YBFE_SetBackgroundColor (MWContext *context,
			   uint8 red,
			   uint8 green,
			   uint8 blue)
{
}

static void 
YBFE_SetCallNetlibAllTheTime (MWContext * win_id)
{
}

static void 
YBFE_ClearCallNetlibAllTheTime (MWContext * win_id)
{
}

static void 
YBFE_GraphProgressInit (MWContext *context,
			  URL_Struct *URL_s,
			  int32 content_length)
{
}

static void 
YBFE_GraphProgressDestroy (MWContext *context,
			     URL_Struct *URL_s,
			     int32 content_length,
			     int32 total_bytes_read)
{
}

static void 
YBFE_GraphProgress (MWContext *context,
		      URL_Struct *URL_s,
		      int32 bytes_received,
		      int32 bytes_since_last_time,
		      int32 content_length)
{
}

static XP_Bool 
YBFE_UseFancyFTP (MWContext * window_id)
{
  return FALSE;
}

static XP_Bool 
YBFE_UseFancyNewsgroupListing (MWContext *window_id)
{
  return FALSE;
}

static int 
YBFE_FileSortMethod (MWContext * window_id)
{
  return -1;
}

static XP_Bool 
YBFE_ShowAllNewsArticles (MWContext *window_id)
{
  return FALSE;
}

static void
YBFE_Alert(MWContext *context,
	     const char *msg)
{
}

static XP_Bool 
YBFE_Confirm(MWContext * context,
	       const char * Msg)
{
  return FALSE;
}

static XP_Bool 
YBFE_CheckConfirm(MWContext *pContext, 
					const char *pConfirmMessage,
					const char *pCheckMessage,
					const char *pOKMessage,
					const char *pCancelMessage,
					XP_Bool *pChecked)
{
  return FALSE;
}

static XP_Bool 
YBFE_SelectDialog(MWContext *pContext,
					const char *pMessage,
					const char **pList,
					int16 *pCount)
{
  return FALSE;
}

static char* 
YBFE_Prompt(MWContext * context,
	      const char * Msg,
	      const char * dflt)
{
  return NULL;
}

static char* 
YBFE_PromptWithCaption(MWContext * context,
			 const char *caption,
			 const char * Msg,
			 const char * dflt)
{
  return NULL;
}

static XP_Bool 
YBFE_PromptUsernameAndPassword (MWContext *context,
				  const char * message,
				  char **username,
				  char **password)
{
  return FALSE;
}

static char* 
YBFE_PromptPassword(MWContext * context,
		      const char * Msg)
{
  return NULL;
}

static void 
YBFE_EnableClicking(MWContext* context)
{
}

static void 
YBFE_AllConnectionsComplete(MWContext * context)
{
}

static void 
YBFE_EraseBackground (MWContext * context,
			int iLocation,
			int32 x,
			int32 y,
			uint32 width,
			uint32 height,
			LO_Color *bg)
{
}

static void 
YBFE_SetDrawable (MWContext *context,
		    CL_Drawable *drawable)
{
}

static void 
YBFE_GetTextFrame (MWContext *context,
		     LO_TextStruct *text,
		     int32 start,
		     int32 end,
		     XP_Rect *frame)
{
}

/* these functions are to allow dealyed native window applet creation and transparent applet */
static void 
YBFE_HandleClippingView (MWContext *pContext,
			   struct LJAppletData *appletD,
			   int x,
			   int y,
			   int width,
			   int height)
{
}

static void 
YBFE_DrawJavaApp (MWContext *pContext,
		    int iLocation,
		    LO_JavaAppStruct *pJava)
{
}

static ContextFuncs _ybfe_funcs = {
#define FE_DEFINE(func, returns, args) YBFE##_##func,
#include "mk_cx_fn.h"
};

MWContext*
YBFE_CreateMWContext()
{
  MWContext *context = XP_NewContext();

  XP_ASSERT(context);
  if (!context) return NULL;

  context->funcs = &_ybfe_funcs;

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
