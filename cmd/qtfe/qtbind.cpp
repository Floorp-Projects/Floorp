/* $Id: qtbind.cpp,v 1.3 1998/10/20 02:40:44 cls%seawood.org Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include <fe_proto.h>
#include "structs.h"
#include "ntypes.h"
#if 0
#include "bkmks.h"
#endif

#include "QtContext.h"
#include "QtBookmarksContext.h"

static void useArgs( const char *fn, ... )
{
    if (0&&fn)
	printf( "%s\n", fn );
}

/* From ./xfe.c: */
extern "C"
MWContext *
FE_GetInitContext(void)
{
  // context->funcs = fe_BuildDisplayFunctionTable();
  useArgs("Ooops... FE_GetInitContext not yet implemented....\n");
  return 0;

}


extern "C"
void
QTFE_HandleClippingView(MWContext *pContext, struct LJAppletData *appletD, int x, int y, int width, int height)
{
    useArgs( "QTFE_HandleClippingView", pContext, appletD, x, y, width, height);
}


/* From ./xfe.c: */
/* Return the copy of the current state of the cipher preference item.
 * Caller is expected to free the returned string.
 */
extern "C"
char *
FE_GetCipherPrefs(void)
{
    useArgs("FE_GetCipherPrefs()");
    return "from FE_GetCipherPrefs";
}

extern "C"
PUBLIC Bool
QTFE_PromptUsernameAndPassword (MWContext * window_id,
				const char *  message,
					  char **       username,
				char **       password)
{
    useArgs("QTFE_PromptUsernameAndPassword (%p,%s,...,...)", window_id, message);
    *username = 0;
    *password = 0;
    return FALSE;
}

/* From ./icons.c: */
/*
 * Return builtin strings for about: displaying
 */
extern "C"
void *
FE_AboutData(const char *which,
	      char **data_ret, int32 *length_ret, char **content_type_ret)
{
    static QString about =
"<html>"
"<title>QtMozilla</title>"
"<body>"
"<h1>QtMozilla</h1>"
"QtMozilla is based on Mozilla!"
;
    *data_ret = about.data();
    *length_ret = about.length();
    *content_type_ret = "text/html";
    return 0;
}

/* From ./icons.c: */
extern "C"
void
FE_FreeAboutData(void *fe_data, const char *which)
{
    // It's static.
}

// This one actually needs to be extern - it's used in qtmoz.cpp
extern "C"
Bool QTFE_Confirm(MWContext * context, const char * Msg)
{
    return QtContext::confirm( context ? QtContext::qt(context) : 0, Msg );
}



extern "C"
Bool
QTFE_UseFancyFTP (MWContext * window_id){
    useArgs( "QTFE_UseFancyFTP ", window_id);
    return 0;
}

extern "C"
Bool
QTFE_UseFancyNewsgroupListing(MWContext *window_id){
    useArgs( "QTFE_UseFancyNewsgroupListing", window_id);
    return 0;
}

extern "C"
XP_Bool
QTFE_ShowAllNewsArticles(MWContext *window_id){
    useArgs( "QTFE_ShowAllNewsArticles", window_id);
    return 0;
}

/* From somewhere... */
extern "C"
MWContext *
QTFE_CreateNewDocWindow(MWContext * calling_context,URL_Struct * URL){
    useArgs( "QTFE_CreateNewDocWindow %p, %p \n", calling_context, URL );
    return 0;
}


extern "C"
Bool
FE_SecurityDialog(MWContext* context, int state, XP_Bool *prefs_toggle)
{
    return QtContext::qt(context)->securityDialog(state, 0 /* prefs_toggle */);
}


extern "C"
void
FE_SetPasswordEnabled(MWContext* context, PRBool usePW)
{
    QtContext::qt(context)->setPasswordEnabled(usePW);
}


extern "C"
void
FE_SetPasswordAskPrefs(MWContext* context, int askPW, int timeout)
{
    QtContext::qt(context)->setPasswordAskPrefs(askPW, timeout);
}


extern "C"
void
FE_SetCipherPrefs(MWContext* context, char *cipher)
{
    QtContext::qt(context)->setCipherPrefs(cipher);
}


extern "C"
void
FE_Alert(MWContext* context, const char *message)
{
    if ( context )
	QtContext::qt(context)->alert(message);
}


extern "C"
void
QTFE_Alert(MWContext* context, const char *message)
{
    QtContext::qt(context)->alert(message);
}


#if !defined(XP_WIN)
/* FE_Message is same as FE_Alert */
extern "C"
void
FE_Message(MWContext* context, const char* message)
{
    QtContext::qt(context)->message(message);
}
#endif


extern "C"
char *
FE_PromptMessageSubject(MWContext* context)
{
    return QtContext::qt(context)->promptMessageSubject();
}


extern "C"
void
FE_RememberPopPassword(MWContext* context, const char *password)
{
    QtContext::qt(context)->rememberPopPassword(password);
}


extern "C"
int
FE_PromptForFileName(MWContext* context, const char *prompt_string,
		      const char *default_path,
		      XP_Bool file_must_exist_p,
		      XP_Bool directories_allowed_p,
		      ReadFileNameCallbackFunction fn,
		      void *closure)
{
    return QtContext::qt(context)->promptForFileName(prompt_string, default_path, file_must_exist_p, directories_allowed_p, fn, closure);
}


extern "C"
char *
QTFE_Prompt(MWContext* context, const char *message, const char *deflt)
{
    return QtContext::qt(context)->prompt(message, deflt);
}


extern "C"
char *
QTFE_PromptWithCaption(MWContext* context, const char *caption,
		      const char *message, const char *deflt)
{
    return QtContext::qt(context)->promptWithCaption(caption, message, deflt);
}


extern "C"
char *
QTFE_PromptPassword(MWContext* context, const char *message)
{
    return QtContext::qt(context)->promptPassword(message);
}


extern "C"
MWContext *
FE_MakeNewWindow(MWContext* context, URL_Struct *url,
		 char       *window_name,
		 Chrome     *chrome)
{
    return QtContext::makeNewWindow(url, window_name, chrome, context);
}

extern "C"
MWContext *
FE_MakeBlankWindow(MWContext *old_context, URL_Struct *url, char *window_name)
{
    return QtContext::makeNewWindow(url, window_name, 0, old_context);
}


extern "C"
void
FE_SetRefreshURLTimer(MWContext* context, URL_Struct *url)
{
    QtContext::qt(context)->setRefreshURLTimer(url->refresh, url->address);
}


extern "C"
void
FE_ConnectToRemoteHost(MWContext* context, int url_type, char *hostname,
			char *port, char *username)
{
    QtContext::qt(context)->connectToRemoteHost(url_type, hostname, port, username);
}


extern "C"
int32
FE_GetContextID(MWContext* context)
{
    return QtContext::qt(context)->getContextID();
}


extern "C"
void
QTFE_CreateEmbedWindow(MWContext* context, NPEmbeddedApp *app)
{
    QtContext::qt(context)->createEmbedWindow(app);
}


extern "C"
void
QTFE_SaveEmbedWindow(MWContext* context, NPEmbeddedApp *app)
{
    QtContext::qt(context)->saveEmbedWindow(app);
}


extern "C"
void
QTFE_RestoreEmbedWindow(MWContext* context, NPEmbeddedApp *app)
{
    QtContext::qt(context)->restoreEmbedWindow(app);
}


extern "C"
void
QTFE_DestroyEmbedWindow(MWContext* context, NPEmbeddedApp *app)
{
    QtContext::qt(context)->destroyEmbedWindow(app);
}


extern "C"
void
QTFE_SetDrawable(MWContext* context, CL_Drawable *drawable)
{
    QtContext::qt(context)->setDrawable(drawable);
}


extern "C"
void
QTFE_Progress(MWContext* context, const char * message)
{
    QtContext::qt(context)->progress(message);
}


extern "C"
void
QTFE_SetCallNetlibAllTheTime(MWContext* context)
{
    QtContext::qt(context)->setCallNetlibAllTheTime();
}


extern "C"
void
QTFE_ClearCallNetlibAllTheTime(MWContext* context)
{
    QtContext::qt(context)->clearCallNetlibAllTheTime();
}


extern "C"
void
QTFE_GraphProgressInit(MWContext* context, URL_Struct *url,
					  int32 content_length)
{
    QtContext::qt(context)->graphProgressInit(url, content_length);
}


extern "C"
void
QTFE_GraphProgressDestroy(MWContext* context, URL_Struct *url,
			int32			content_length,
			int32			total_bytes_read)
{
    QtContext::qt(context)->graphProgressDestroy(url, content_length, total_bytes_read);
}


extern "C"
void
QTFE_GraphProgress(MWContext* context, URL_Struct *url,
				  int32 p2,
				  int32 bytes_since_last_time,
				  int32 p4)
{
    QtContext::qt(context)->graphProgress(url,
				  p2 , bytes_since_last_time,
				  p4 );
}


extern "C"
int
QTFE_FileSortMethod(MWContext* context)
{
    return QtContext::qt(context)->fileSortMethod();
}


extern "C"
void
QTFE_LayoutNewDocument(MWContext* context, URL_Struct *url,
		      int32 *iWidth, int32 *iHeight,
		       int32 *mWidth, int32 *mHeight)
{
    QtContext::qt(context)->layoutNewDocument(url, iWidth, iHeight, mWidth, mHeight);
}


extern "C"
void
QTFE_SetDocTitle(MWContext* context, char *title)
{
    QtContext::qt(context)->setDocTitle(title);
}


extern "C"
void
QTFE_FinishedLayout(MWContext* context)
{
    QtContext::qt(context)->finishedLayout();
}


extern "C"
void
QTFE_BeginPreSection(MWContext* context)
{
    QtContext::qt(context)->beginPreSection();
}


extern "C"
void
QTFE_EndPreSection(MWContext* context)
{
    QtContext::qt(context)->endPreSection();
}


extern "C"
char *
QTFE_TranslateISOText(MWContext* context, int charset, char *ISO_Text)
{
    return QtContext::qt(context)->translateISOText(charset, ISO_Text);
}


extern "C"
int
QTFE_GetTextInfo(MWContext* context, LO_TextStruct *text,
		LO_TextInfo *text_info)
{
    QtContext::qt(context)->getTextInfo(text, text_info);

    return 1; // Netscape expects a return value, so fake one.
}


extern "C"
void
QTFE_GetTextFrame(MWContext* context, LO_TextStruct *text, int32 start,
                      int32 end, XP_Rect *frame)
{
    QtContext::qt(context)->getTextFrame(text, start, end, frame);
}


extern "C"
void
QTFE_GetEmbedSize(MWContext* context, LO_EmbedStruct *embed_struct,
		  NET_ReloadMethod force_reload)
{
    QtContext::qt(context)->getEmbedSize(embed_struct, force_reload);
}


extern "C"
void
QTFE_GetJavaAppSize(MWContext* context, LO_JavaAppStruct *java_struct,
		    NET_ReloadMethod reloadMethod)
{
    QtContext::qt(context)->getJavaAppSize(java_struct, reloadMethod);
}


extern "C"
void
QTFE_GetFormElementInfo(MWContext* context, LO_FormElementStruct *form)
{
    QtContext::qt(context)->getFormElementInfo(form);
}


extern "C"
void
QTFE_GetFormElementValue(MWContext* context, LO_FormElementStruct *form,
					XP_Bool delete_p, XP_Bool submit_p)
{
    QtContext::qt(context)->getFormElementValue(form, delete_p, submit_p);
}


extern "C"
void
QTFE_ResetFormElement(MWContext* context, LO_FormElementStruct *form)
{
    QtContext::qt(context)->resetFormElement(form);
}


extern "C"
void
QTFE_SetFormElementToggle(MWContext* context, LO_FormElementStruct *form,
			  XP_Bool state)
{
    QtContext::qt(context)->setFormElementToggle(form, state);
}


extern "C"
void
QTFE_FreeEmbedElement(MWContext* context, LO_EmbedStruct *embed_struct)
{
    QtContext::qt(context)->freeEmbedElement(embed_struct);
}


extern "C"
void
QTFE_FreeJavaAppElement(MWContext* context, struct LJAppletData *appletData)
{
    QtContext::qt(context)->freeJavaAppElement(appletData);
}


extern "C"
void
QTFE_HideJavaAppElement(MWContext* context, struct LJAppletData *appletData)
{
    QtContext::qt(context)->hideJavaAppElement(appletData);
}


extern "C"
void
QTFE_FreeEdgeElement(MWContext* context, LO_EdgeStruct *edge)
{
    QtContext::qt(context)->freeEdgeElement(edge);
}


extern "C"
void
QTFE_FormTextIsSubmit(MWContext* context, LO_FormElementStruct *form)
{
    QtContext::qt(context)->formTextIsSubmit(form);
}


extern "C"
void
QTFE_SetProgressBarPercent(MWContext* context, int32 percent)
{
    QtContext::qt(context)->setProgressBarPercent(percent);
}


extern "C"
void
QTFE_SetBackgroundColor(MWContext* context, uint8 red, uint8 green,
			uint8 blue)
{
    QtContext::qt(context)->setBackgroundColor(red, green, blue);
}


extern "C"
void
QTFE_DisplaySubtext(MWContext* context, int iLocation,
		   LO_TextStruct *text, int32 start_pos, int32 end_pos,
		   XP_Bool need_bg)
{
    QtContext::qt(context)->displaySubtext(iLocation, text, start_pos, end_pos, need_bg);
}


extern "C"
void
QTFE_DisplayText(MWContext* context, int iLocation, LO_TextStruct *text,
	XP_Bool need_bg)
{
    QtContext::qt(context)->displayText(iLocation, text, need_bg);
}


extern "C"
void
QTFE_DisplayEmbed(MWContext* context, int iLocation, LO_EmbedStruct *embed_struct)
{
    QtContext::qt(context)->displayEmbed(iLocation, embed_struct);
}


extern "C"
void
QTFE_DisplayJavaApp(MWContext* context, int iLocation, LO_JavaAppStruct *java_struct)
{
    QtContext::qt(context)->displayJavaApp(iLocation, java_struct);
}


extern "C"
void
QTFE_DisplayEdge(MWContext* context, int loc, LO_EdgeStruct *edge)
{
    QtContext::qt(context)->displayEdge(loc, edge);
}


extern "C"
void
QTFE_DisplayTable(MWContext* context, int loc, LO_TableStruct *ts)
{
    QtContext::qt(context)->displayTable(loc, ts);
}


extern "C"
void
QTFE_DisplayCell(MWContext* context, int loc, LO_CellStruct *cell)
{
    QtContext::qt(context)->displayCell(loc, cell);
}


extern "C"
void
QTFE_DisplaySubDoc(MWContext* context, int loc, LO_SubDocStruct *sd)
{
    QtContext::qt(context)->displaySubDoc(loc, sd);
}


extern "C"
void
QTFE_DisplayLineFeed(MWContext* context, int iLocation, LO_LinefeedStruct *line_feed, XP_Bool need_bg)
{
    QtContext::qt(context)->displayLineFeed(iLocation, line_feed, need_bg);
}


extern "C"
void
QTFE_DisplayHR(MWContext* context, int iLocation, LO_HorizRuleStruct *hr)
{
    QtContext::qt(context)->displayHR(iLocation, hr);
}


extern "C"
void
QTFE_DisplayBullet(MWContext* context, int iLocation, LO_BulletStruct *bullet)
{
    QtContext::qt(context)->displayBullet(iLocation, bullet);
}


extern "C"
void
QTFE_DisplayFormElement(MWContext* context, int iLocation,
					   LO_FormElementStruct *form)
{
    QtContext::qt(context)->displayFormElement(iLocation, form);
}


extern "C"
void
QTFE_DisplayBorder(MWContext* context, int iLocation, int x, int y, int width,
                  int height, int bw, LO_Color *color, LO_LineStyle style)
{
    QtContext::qt(context)->displayBorder(iLocation, x, y, width, height, bw, color, style);
}


extern "C"
void
QTFE_DisplayFeedback(MWContext* context, int iLocation, LO_Element *element)
{
    QtContext::qt(context)->displayFeedback(iLocation, element);
}


extern "C"
void
QTFE_EraseBackground(MWContext* context, int iLocation, int32 x, int32 y,
                    uint32 width, uint32 height, LO_Color *bg)
{
    QtContext::qt(context)->eraseBackground(iLocation, x, y, width, height, bg);
}


extern "C"
void
QTFE_ClearView(MWContext* context, int which)
{
    QtContext::qt(context)->clearView(which);
}


extern "C"
void
QTFE_SetDocDimension(MWContext* context, int iLocation, int32 iWidth, int32 iLength)
{
    QtContext::qt(context)->setDocDimension(iLocation, iWidth, iLength);
}


extern "C"
void
QTFE_SetDocPosition(MWContext* context, int iLocation, int32 x, int32 y)
{
    QtContext::qt(context)->setDocPosition(iLocation, x, y);
}


extern "C"
void
QTFE_GetDocPosition(MWContext* context, int iLocation,
		   int32 *iX, int32 *iY)
{
    QtContext::qt(context)->getDocPosition(iLocation, iX, iY);
}


extern "C"
void
QTFE_EnableClicking(MWContext* context)
{
    QtContext::qt(context)->enableClicking();
}


extern "C"
void
QTFE_DrawJavaApp(MWContext* context, int iLocation, LO_JavaAppStruct *java_struct)
{
    QtContext::qt(context)->drawJavaApp(iLocation, java_struct);
}


extern "C"
void
QTFE_AllConnectionsComplete(MWContext* context)
{
    QtContext::qt(context)->allConnectionsComplete();
}

extern "C"
void
QTFE_FreeBuiltinElement(MWContext *, LO_BuiltinStruct *)
{
}

extern "C"
void
QTFE_DisplayBuiltin(MWContext *, int  ,LO_BuiltinStruct *)
{
}


extern "C"
void
FE_ReleaseTextAttrFeData(MWContext* context, LO_TextAttr *attr)
{
    QtContext::qt(context)->releaseTextAttrFeData(attr);
}


extern "C"
void
FE_FreeFormElement(MWContext* context, LO_FormElementData *form_data)
{
    QtContext::qt(context)->freeFormElement(form_data);
}


extern "C"
int
FE_EnableBackButton(MWContext* context)
{
    return QtContext::qt(context)->enableBackButton();
}


extern "C"
int
FE_EnableForwardButton(MWContext* context)
{
    return QtContext::qt(context)->enableForwardButton();
}


extern "C"
int
FE_DisableBackButton(MWContext* context)
{
    return QtContext::qt(context)->disableBackButton();
}


extern "C"
int
FE_DisableForwardButton(MWContext* context)
{
    return QtContext::qt(context)->disableForwardButton();
}


extern "C"
void
FE_UpdateStopState(MWContext* context)
{
    QtContext::qt(context)->updateStopState();
}


extern "C"
void *
FE_FreeGridWindow(MWContext* context, XP_Bool save_history)
{
    return QtContext::qt(context)->freeGridWindow(save_history);
}


extern "C"
void
FE_RestructureGridWindow(MWContext* context, int32 x, int32 y,
        int32 width, int32 height)
{
    QtContext::qt(context)->restructureGridWindow(x, y, width, height);
}


extern "C"
void
FE_GetFullWindowSize(MWContext* context, int32 *width, int32 *height)
{
    QtContext::qt(context)->getFullWindowSize(width, height);
}


#if defined(XP_UNIX)
extern "C"
void
FE_GetEdgeMinSize(MWContext* context, int32 *size_p)
{
    QtContext::qt(context)->getEdgeMinSize(size_p);
}
#endif

#if defined(XP_WIN)
extern "C"
void
FE_GetEdgeMinSize(MWContext *context, int32 *size_p, Bool /* no_edge */ )
{
    QtContext::qt(context)->getEdgeMinSize(size_p);
}
#endif

extern "C"
void
FE_LoadGridCellFromHistory(MWContext* context, void *hist,
			   NET_ReloadMethod force_reload)
{
    QtContext::qt(context)->loadGridCellFromHistory(hist, force_reload);
}


extern "C"
void
FE_ShiftImage(MWContext* context, LO_ImageStruct *lo_image)
{
    QtContext::qt(context)->shiftImage(lo_image);
}


extern "C"
void
FE_ScrollDocTo(MWContext* context, int iLocation, int32 x, int32 y)
{
    QtContext::qt(context)->scrollDocTo(iLocation, x, y);
}


extern "C"
void
FE_ScrollDocBy(MWContext* context, int iLocation, int32 deltax, int32 deltay)
{
    QtContext::qt(context)->scrollDocBy(iLocation, deltax, deltay);
}


extern "C"
void
FE_BackCommand(MWContext* context)
{
    QtContext::qt(context)->backCommand();
}


extern "C"
void
FE_ForwardCommand(MWContext* context)
{
    QtContext::qt(context)->forwardCommand();
}


extern "C"
void
FE_HomeCommand(MWContext* context)
{
    QtContext::qt(context)->homeCommand();
}


extern "C"
void
FE_PrintCommand(MWContext* context)
{
    QtContext::qt(context)->printCommand();
}


extern "C"
void
FE_GetWindowOffset(MWContext* context, int32 *sx, int32 *sy)
{
    QtContext::qt(context)->getWindowOffset(sx, sy);
}


extern "C"
void
FE_GetScreenSize(MWContext* context, int32 *sx, int32 *sy)
{
    QtContext::qt(context)->getScreenSize(sx, sy);
}

extern "C"
void
FE_GetAvailScreenRect (MWContext *context, int32 *sx, int32 *sy,
		       int32 *left, int32 *top)
{
    QtContext::qt(context)->getAvailScreenRect(sx, sy,left,top);
}


extern "C"
void
FE_GetPixelAndColorDepth(MWContext* context, int32 *pixelDepth,
			 int32 *colorDepth)
{
    QtContext::qt(context)->getPixelAndColorDepth(pixelDepth, colorDepth);
}


extern "C"
void
FE_SetWindowLoading(MWContext* context, URL_Struct *url,
	Net_GetUrlExitFunc **exit_func_p)
{
    QtContext::qt(context)->setWindowLoading(url, exit_func_p);
}


extern "C"
void
FE_RaiseWindow(MWContext* context)
{
    QtContext::qt(context)->raiseWindow();
}


extern "C"
void
FE_DestroyWindow(MWContext* context)
{
    QtContext::qt(context)->destroyWindow();
}


extern "C"
void
FE_GetDocAndWindowPosition(MWContext* context, int32 *pX, int32 *pY,
    int32 *pWidth, int32 *pHeight )
{
    QtContext::qt(context)->getDocAndWindowPosition(pX, pY, pWidth, pHeight);
}


extern "C"
PUBLIC void
FE_DisplayTextCaret(MWContext* context, int iLocation, LO_TextStruct* text,
		    int char_offset)
{
    QtContext::qt(context)->displayTextCaret(iLocation, text, char_offset);
}


extern "C"
void
FE_DisplayImageCaret(MWContext* context, LO_ImageStruct*        image,
		     ED_CaretObjectPosition caretPos)
{
    QtContext::qt(context)->displayImageCaret(image, caretPos);
}


extern "C"
void
FE_DisplayGenericCaret(MWContext* context, LO_Any * image,
                        ED_CaretObjectPosition caretPos )
{
    QtContext::qt(context)->displayGenericCaret(image,
                        caretPos );
}


extern "C"
Bool
FE_GetCaretPosition(MWContext* context, LO_Position* where,
		    int32* caretX, int32* caretYLow, int32* caretYHigh
)
{
    return QtContext::qt(context)->getCaretPosition(where, caretX, caretYLow, caretYHigh);
}


extern "C"
PUBLIC void
FE_DestroyCaret(MWContext* context)
{
    QtContext::qt(context)->destroyCaret();
}


extern "C"
PUBLIC void
FE_ShowCaret(MWContext* context)
{
    QtContext::qt(context)->showCaret();
}


extern "C"
PUBLIC void
FE_DocumentChanged(MWContext* context, int32 p_y, int32 p_height)
{
    QtContext::qt(context)->documentChanged(p_y, p_height);
}


extern "C"
void
FE_SetNewDocumentProperties(MWContext* context)
{
    QtContext::qt(context)->setNewDocumentProperties();
}


extern "C"
void
FE_ImageLoadDialog(MWContext* context)
{
    QtContext::qt(context)->imageLoadDialog();
}


extern "C"
void
FE_ImageLoadDialogDestroy(MWContext* context)
{
    QtContext::qt(context)->imageLoadDialogDestroy();
}


extern "C"
void
FE_SaveDialogCreate(MWContext* context, int nfiles, ED_SaveDialogType saveType)
{
    QtContext::qt(context)->saveDialogCreate(nfiles, saveType);
}


extern "C"
void
FE_SaveDialogSetFilename(MWContext* context, char* filename)
{
    QtContext::qt(context)->saveDialogSetFilename(filename);
}


extern "C"
void
FE_FinishedSave(MWContext* context, int status,
		char *pDestURL, int iFileNumber)
{
    QtContext::qt(context)->finishedSave(status, pDestURL, iFileNumber);
}


extern "C"
void
FE_SaveDialogDestroy(MWContext* context, int status, char* file_url)
{
    QtContext::qt(context)->saveDialogDestroy(status, file_url);
}


extern "C"
Bool
FE_SaveErrorContinueDialog(MWContext* context, char*        filename,
			   ED_FileError error)
{
    return QtContext::qt(context)->saveErrorContinueDialog(filename, error);
}


extern "C"
void
FE_ClearBackgroundImage(MWContext* context)
{
    QtContext::qt(context)->clearBackgroundImage();
}


extern "C"
void
FE_EditorDocumentLoaded(MWContext* context)
{
    QtContext::qt(context)->editorDocumentLoaded();
}


extern "C"
void
FE_DisplayAddRowOrColBorder(MWContext* context, XP_Rect *pRect,
                            XP_Bool bErase)
{
    QtContext::qt(context)->displayAddRowOrColBorder(pRect, bErase);
}


extern "C"
void
FE_DisplayEntireTableOrCell(MWContext* context, LO_Element* pLoElement)
{
    QtContext::qt(context)->displayEntireTableOrCell(pLoElement);
}


extern "C"
int
FE_GetURL(MWContext* context, URL_Struct *url)
{
    return QtContext::qt(context)->getURL(url);
}


extern "C"
void
FE_FocusInputElement(MWContext* context, LO_Element *element)
{
    QtContext::qt(context)->focusInputElement(element);
}


extern "C"
void
FE_BlurInputElement(MWContext* context, LO_Element *element)
{
    QtContext::qt(context)->blurInputElement(element);
}


extern "C"
void
FE_SelectInputElement(MWContext* context, LO_Element *element)
{
    QtContext::qt(context)->selectInputElement(element);
}


extern "C"
void
FE_ChangeInputElement(MWContext* context, LO_Element *element)
{
    QtContext::qt(context)->changeInputElement(element);
}


extern "C"
void
FE_SubmitInputElement(MWContext* context, LO_Element *element)
{
    QtContext::qt(context)->submitInputElement(element);
}


extern "C"
void
FE_ClickInputElement(MWContext* context, LO_Element *xref)
{
    QtContext::qt(context)->clickInputElement(xref);
}


extern "C"
PRBool
FE_HandleLayerEvent(MWContext* context, CL_Layer *layer,
                           CL_Event *layer_event)
{
    return (PRBool)QtContext::qt(context)->handleLayerEvent(layer, layer_event);
}


extern "C"
PRBool
FE_HandleEmbedEvent(MWContext* context, LO_EmbedStruct *embed,
                           CL_Event *event)
{
    return (PRBool)QtContext::qt(context)->handleEmbedEvent(embed, event);
}


extern "C"
void
FE_UpdateChrome(MWContext* context, Chrome *chrome)
{
    QtContext::qt(context)->updateChrome(chrome);
}


extern "C"
void
FE_QueryChrome(MWContext* context, Chrome * chrome)
{
    QtContext::qt(context)->queryChrome(chrome);
}


extern "C"
MWContext *
FE_MakeGridWindow(MWContext* context, void *hist_list, void *history,
	int32 x, int32 y, int32 width, int32 height,
	char *url_str, char *window_name, int8 scrolling,
	NET_ReloadMethod force_reload, Bool no_edge)
{
    //debug("makeGridWindow");
    return QtContext::qt(context)->makeGridWindow(hist_list, history, x, y, width, height, url_str, window_name, scrolling,
	force_reload, no_edge);
}


extern "C"
void
FE_ClearDNSSelect(MWContext* context, int socket)
{
    QtContext::qt(context)->clearDNSSelect(socket);
}


extern "C"
void
FE_SetReadSelect(MWContext* context, int fd)
{
    QtContext::qt(context)->setReadSelect(fd);
}


extern "C"
void
FE_ClearReadSelect(MWContext* context, int fd)
{
    QtContext::qt(context)->clearReadSelect(fd);
}


extern "C"
void
FE_SetConnectSelect(MWContext* context, int fd)
{
    QtContext::qt(context)->setConnectSelect(fd);
}


extern "C"
void
FE_ClearConnectSelect(MWContext* context, int fd)
{
    QtContext::qt(context)->clearConnectSelect(fd);
}


extern "C"
void
FE_SetFileReadSelect(MWContext* context, int fd)
{
    QtContext::qt(context)->setFileReadSelect(fd);
}


extern "C"
void
FE_ClearFileReadSelect(MWContext* context, int fd)
{
    QtContext::qt(context)->clearFileReadSelect(fd);
}


extern "C"
char*
FE_GetTempFileFor(MWContext* /*context*/, const char* fname,
			   XP_FileType ftype, XP_FileType* rettype)
{
  char* actual = WH_FileName(fname, ftype);
  int len;
  char* result;
  if (!actual) return NULL;
  len = strlen(actual) + 10;
  result = (char*)XP_ALLOC(len);
  if (!result) return NULL;
  PR_snprintf(result, len, "%s-XXXXXX", actual);
  XP_FREE(actual);
  mktemp(result);
  *rettype = xpMailFolder;	/* Ought to be harmless enough. */
  return result;
}

extern "C"
XP_Bool
QTFE_CheckConfirm (MWContext *pContext, const char *pConfirmMessage,
		 const char *pCheckMessage, const char *pOKMessage,
		 const char *pCancel, XP_Bool *pChecked)
{
   XP_Bool userHasAccepted = QTFE_Confirm (pContext, pConfirmMessage);
   *pChecked = QTFE_Confirm (pContext, pCheckMessage); 
   return userHasAccepted;
}

extern "C"
XP_Bool
QTFE_SelectDialog (MWContext *pContext, const char *pMessage,
		 const char **pList, int16 *pCount)
{
    int i;
    char *message = 0;
    for (i = 0; i < *pCount; i++) {
	StrAllocCopy(message, pMessage);
        StrAllocCat(message, " = ");
	StrAllocCat(message, pList[i]);
	if (QTFE_Confirm(pContext, message)) {
	    /* user selected this one */
	    XP_FREE(message);
	    *pCount = i;
	    return TRUE;
	}
    }

    /* user rejected all */
    XP_FREE(message);
    return FALSE;
}

ContextFuncs qtbind= {
#define FE_DEFINE(func, returns, args) QTFE##_##func,
#include "mk_cx_fn.h"
};

/* Binding of BMFE (bookmark functions) starts here. */

extern "C"
void  BMFE_RefreshCells (MWContext* context, int32 first, int32 last,
							  XP_Bool now)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->refreshCells( first,last, now );
}

extern "C"
void  BMFE_SyncDisplay (MWContext* context)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->syncDisplay();
}

extern "C"
void  BMFE_MeasureEntry (MWContext* context, BM_Entry* entry,
			 uint32* width, uint32* height)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->measureEntry( entry, width, height );
}

extern "C"
void  BMFE_SetClipContents (MWContext* context, void* buffer,
			    int32 length)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->setClipContents( buffer, length );
}

extern "C"
void*  BMFE_GetClipContents (MWContext* context, int32* length)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    return bm ? bm->getClipContents( length ) : 0;
}

extern "C"
void  BMFE_OpenBookmarksWindow (MWContext* context)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->openBookmarksWindow();
}

extern "C"
void  BMFE_EditItem (MWContext* context, BM_Entry* entry)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->editItem( entry );
}

extern "C"
void  BMFE_EntryGoingAway (MWContext* context, BM_Entry* entry)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->entryGoingAway( entry );
}

extern "C"
void  BMFE_GotoBookmark (MWContext* context,
			 const char* url, const char* target)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->gotoBookmark( url, target );
}

extern "C"
void*  BMFE_OpenFindWindow (MWContext* context, BM_FindInfo* findInfo)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    return bm ? bm->openFindWindow( findInfo ) : 0;
}

extern "C"
void  BMFE_ScrollIntoView (MWContext* context, BM_Entry* entry)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->scrollIntoView( entry );
}

extern "C"
void  BMFE_BookmarkMenuInvalid (MWContext* context)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->bookmarkMenuInvalid();
}

extern "C"
void  BMFE_UpdateWhatsChanged (MWContext* context,
			       const char* url, /* If NULL, just display
						   "Checking..." */
			       int32 done, int32 total,
			       const char* totaltime)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->updateWhatsChanged( url, done, total, totaltime );
}

extern "C"
void  BMFE_FinishedWhatsChanged (MWContext* context, int32 totalchecked,
				 int32 numreached, int32 numchanged)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->finishedWhatsChanged( totalchecked, numreached, numchanged );
}

extern "C"
void  BMFE_StartBatch (MWContext* context)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->startBatch();
}

extern "C"
void  BMFE_EndBatch (MWContext* context)
{
    QtBookmarksContext *bm = QtBookmarksContext::qt(context);
    if ( bm )
	bm->endBatch();
}

extern "C"
ED_CharsetEncode FE_EncodingDialog(MWContext* context, char* newCharset)
{
    ED_CharsetEncode retval = ED_ENCODE_CANCEL;

	/* Write me */
	assert( 0 );

    return retval;
}

// Mail specific FE functions
// Included because of compilation problems
#include <msgcom.h>
extern "C"
MSG_Pane*
FE_CreateCompositionPane(MWContext* old_context,
                         MSG_CompositionFields* fields,
                         const char* initialText,
                         MSG_EditorType editorType)
{
   return 0;
}

extern "C"
void
FE_UpdateCompToolbar(MSG_Pane* comppane)
{
}

extern "C"
const char *
FE_UsersOrganization()
{
   return 0;
}

extern "C"
void
FE_DestroyMailCompositionContext(MWContext* context)
{
}

extern "C"
void
FE_MsgShowHeaders(MSG_Pane *pPane,
                  MSG_HEADER_SET mhsHeaders)
{
}

extern "C"
const char *
FE_UsersRealMailAddress()
{
   return 0;
}

#ifdef XP_UNIX
extern "C" 
void XFE_InitializePrintSetup (PrintSetup *p)
{
}
#endif /* XP_UNIX */


/* If we're set up to deliver mail/news by running a program rather
   than by talking to SMTP/NNTP, this does it.
   
   Returns positive if delivery via program was successful;
   Returns negative if delivery failed;
   Returns 0 if delivery was not attempted (in which case we
   should use SMTP/NNTP instead.)
 
   $NS_MSG_DELIVERY_HOOK names a program which is invoked with one argument,
   a tmp file containing a message.  (Lines are terminated with CRLF.)
   This program is expected to parse the To, CC, BCC, and Newsgroups headers,
   and effect delivery to mail and/or news.  It should exit with status 0
    iff successful.
    
    #### This really wants to be defined in libmsg, but it wants to
    be able to use fe_perror, so...
*/
extern "C"
int
msg_DeliverMessageExternally(MWContext *context, const char *msg_file)
{
}
