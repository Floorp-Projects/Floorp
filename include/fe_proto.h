/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#ifndef _FrontEnd_
#define _FrontEnd_
 
#include "net.h"
#include "xp_obs.h"
#include "il_types.h"
#include "ctxtfunc.h"

#include "prio.h"     /* for netlib function decls */
#include "prnetdb.h"  /* for netlib function decls */

XP_BEGIN_PROTOS

/* Carriage return and linefeeds */

#define CR '\015'
#define LF '\012'
#define VTAB '\013'
#define FF '\014'
#define TAB '\011'
#define CRLF "\015\012"     /* A CR LF equivalent string */

#ifdef XP_MAC
#  define LINEBREAK             "\012"
#  define LINEBREAK_LEN 1
#else
#  if defined(XP_WIN) || defined(XP_OS2)
#    define LINEBREAK           "\015\012"
#    define LINEBREAK_LEN       2
#  else
#    ifdef XP_UNIX
#      define LINEBREAK         "\012"
#      define LINEBREAK_LEN     1
#    endif /* XP_UNIX */
#  endif /* XP_WIN */
#endif /* XP_MAC */

/* set a timer and load the specified URL after the
 * timer has elapsed.  Cancel the timer if the user
 * leaves the current page
 */
extern void FE_SetRefreshURLTimer(MWContext *context, 
								  uint32     seconds, 
								  char      *refresh_url);

#ifdef _USRDLL
extern void NDLLFE_SetRefreshURLTimer(MWContext *context, 
								  uint32     seconds, 
								  char      *refresh_url);
#endif

/* this function should register a function that will
 * be called after the specified interval of time has
 * elapsed.  This function should return an id 
 * that can be passed to FE_ClearTimeout to cancel 
 * the Timeout request.
 *
 * A) Timeouts never fail to trigger, and
 * B) Timeouts don't trigger *before* their nominal timestamp expires, and
 * C) Timeouts trigger in the same ordering as their timestamps
 *
 * After the function has been called it is unregistered
 * and will not be called again unless re-registered.
 *
 * func:    The function to be invoked upon expiration of
 *          the Timeout interval 
 * closure: Data to be passed as the only argument to "func"
 * msecs:   The number of milli-seconds in the interval
 */
typedef void
(*TimeoutCallbackFunction) (void * closure);

extern void * 
FE_SetTimeout(TimeoutCallbackFunction func, void * closure, uint32 msecs);

/* This function cancels a Timeout that has previously been
 * set.  
 * Callers should not pass in NULL or a timer_id that
 * has already expired.
 */
extern void 
FE_ClearTimeout(void *timer_id);

/* set and clear select fd's
 */
#ifdef XP_UNIX
extern void FE_SetReadPoll(PRFileDesc *fd);
extern void FE_ClearReadPoll(PRFileDesc *fd);
extern void FE_SetConnectPoll(PRFileDesc *fd);
extern void FE_ClearConnectPoll(PRFileDesc *fd);
#endif /* XP_UNIX */

/* do async dns */
extern int FE_AsyncDNSLookup(MWContext *context, 
				             char * host_port,
				             PRHostEnt ** hoststruct_ptr_ptr, 
	                         PRFileDesc *socket);
extern void FE_AbortDNSLookup(PRFileDesc *socket);

/* all the other selects are depricated and should be removed shortly */

/* tell the front end to call ProcessNet with fd when fd
 * has data ready for reading
 */
extern void FE_SetDNSSelect(MWContext * win_id, int fd);
extern void FE_ClearDNSSelect(MWContext * win_id, int fd);

/* set and clear select fd's
 */
/* tell the front end to call ProcessNet with fd when fd
 * has data ready for reading
 */
extern void FE_SetReadSelect(MWContext * win_id, int fd);
extern void FE_ClearReadSelect(MWContext * win_id, int fd);

/* tell the front end to call ProcessNet with fd when fd
 * has connected. (write select and exception select)
 */
extern void FE_SetConnectSelect(MWContext * win_id, int fd);
extern void FE_ClearConnectSelect(MWContext * win_id, int fd);

/* tell the front end to call ProcessNet with fd whenever fd
 * has data ready for reading
 */
extern void FE_SetFileReadSelect(MWContext * win_id, int fd);
extern void FE_ClearFileReadSelect(MWContext * win_id, int fd);

/* tell the front end to call ProcessNet as often as possible
 * This superseeds FE_SetCallNetlibAllTheTime
 */
extern void FE_SetCallNetlibAllTheTime(MWContext * win_id);
/* Same as,FE_SetCallNetlibAllTheTime but not in context's function table */
extern void XP_SetCallNetlibAllTheTime(MWContext * win_id);	

/* tell the front end to stop calling ProcessNet as often as possible
 * This superseeds FE_ClearCallNetlibAllTheTime
 */
extern void FE_ClearCallNetlibAllTheTime(MWContext * win_id);
/* Same as,FE_SetCallNetlibAllTheTime but not in context's function table */
extern void XP_ClearCallNetlibAllTheTime(MWContext * win_id);	

/* use telnet, rlogin or tn3270 to connect to a remote host
 * this function should be non_blocking if you want your front
 * end to continue working.
 */
#define FE_TELNET_URL_TYPE 1
#define FE_TN3270_URL_TYPE 2
#define FE_RLOGIN_URL_TYPE 3
PUBLIC void FE_ConnectToRemoteHost(MWContext * ctxt, int url_type, char *hostname, char * port,  char *username);
#ifdef _USRDLL
PUBLIC void NDLLFE_ConnectToRemoteHost(MWContext * ctxt, int url_type, char *hostname, char * port,  char *username);
#endif

/* returns the users mail address as a constant string.  If not known the
 * routine should return NULL.
 */
extern const char * FE_UsersMailAddress(void);
/* returns the users full name as a constant string.  If not known the
 * routine should return NULL.
 */
extern const char * FE_UsersFullName(void);

/* returns the contents of the users signature file as a constant string.
 * If not known the routine should return NULL.
 * The signature data should end with a single newline.
 */
extern const char *FE_UsersSignature(void);

/* puts up a FE security dialog
 *
 * Should return TRUE if the url should continue to
 * be loaded
 * Should return FALSE if the url should be aborted
 */
extern Bool FE_SecurityDialog(MWContext * context, int message, XP_Bool *prefs_toggle);
#ifdef _USRDLL
extern Bool NDLLFE_SecurityDialog(MWContext * context, int message);
#endif

#define SD_INSECURE_POST_FROM_SECURE_DOC              1
#define SD_INSECURE_POST_FROM_INSECURE_DOC            2
#define SD_ENTERING_SECURE_SPACE                      3
#define SD_LEAVING_SECURE_SPACE                       4
#define SD_INSECURE_DOCS_WITHIN_SECURE_DOCS_NOT_SHOWN 5
#define SD_REDIRECTION_TO_INSECURE_DOC                6
#define SD_REDIRECTION_TO_SECURE_SITE                 7

/*
** Pass the password related preferences from the security library to the FE.
**      "cx" is the window context
**      "usePW" is true for "use a password", false otherwise
**      "askPW" is when to ask for the password:
**              -1 = every time its needed
**              0  = once per session
**              1  = after 'n' minutes of inactivity
**      "timeout" is the number of inactive minutes to forget the password
*/
extern void FE_SetPasswordPrefs(MWContext *context,
								PRBool usePW, int askPW, int timeout);

/*
 * Inform the FE that the security library is or is not using a password.
 *      "cx" is the window context
 *      "usePW" is true for "use a password", false otherwise
 */
extern void FE_SetPasswordEnabled(MWContext *context, PRBool usePW);

/*
 * Inform the FE that the user has chosen when-to-ask-for-password preferences.
 *      "cx" is the window context
 *      "askPW" is when to ask for the password:
 *              -1 = every time its needed
 *               0 = once per session
 *               1 = after 'n' minutes of inactivity
 *      "timeout" is the number of inactive minutes to forget the password
 *              (this value should be ignored unless askPW is 1)
 */
extern void FE_SetPasswordAskPrefs(MWContext *context, int askPW, int timeout);

/* Cipher preference handling:
 *
 * Sets the cipher preference item in the FE and saves the preference file.
 * The FE will take a copy of passed in cipher argument.
 * If context is NULL, FE will choose an appropriate context if neccessary.
 */
void FE_SetCipherPrefs(MWContext *context, char *cipher);

/* Return the copy of the current state of the cipher preference item.
 * Caller is expected to free the returned string.
 */
char * FE_GetCipherPrefs(void);

/* causes the application to display a non modal mail/news editing window
 *
 * after the completion of mail editing the function controling the
 * mail window should package the data as post data and call
 * NET_GetURL with a mailto URL.  Any mail headers should be contained within
 * URL_Struct->post_headers and the body of the mail message within
 * the URL_Struct->post_data.
 *
 * if the newsgroups field is non-empty the same post data used for the
 * call to the smtp code should be called with a newspost url.
 * the news url minus the newsgroups is given as the "news_url" argument
 * of this function.  If the newsgroups line is "comp.infosystems.www"
 * then it should just be strcat'd onto the end of the "news_url"
 * argument and passed to netlib.
 */
extern void FE_EditMailMessage(MWContext *context, 
							   const char * to_address,
							   const char * subject,
							   const char * newsgroups,
							   const char * references,
							   const char * news_url);


/* graph the progress of a transfer with a bar of
 * some sort.  This will be called in place
 * of FE_Progress when a content length is known
 */
extern void FE_GraphProgressInit    (MWContext  *context, 
								 URL_Struct *URL_s, 
								 int32       content_length);
extern void FE_GraphProgressDestroy (MWContext  *context, 
								 URL_Struct *URL_s, 
								 int32       content_length,
								     int32       total_bytes_read);

extern void FE_GraphProgress        (MWContext  *context, 
								 URL_Struct *URL_s, 
								 int32       bytes_received,
								 int32       bytes_since_last_time,
								 int32       content_length);

/*      When the netlib or a netlib stream change state, they should inform the
	front end so it can provide feedback */
typedef enum _Net_RequestStatus {
	nsStarted, nsConnected, nsResolved, nsProgress, nsFinished, nsAborted
}       Net_RequestStatus;
extern void FE_NetStatus (MWContext *context, void *request, Net_RequestStatus status);

/*      netlib and netlib streams will use this to keep the front end informed of
	state changes but this information does not specify which request it
	refers to, and the text message is only good for user text feedback */
extern void FE_Progress (MWContext *context, const char * Msg);

extern void FE_Alert (MWContext * context, const char * Msg);

#if defined(XP_MAC)||defined(XP_UNIX)
extern void FE_Message (MWContext * context, const char * Msg);
#else
#define FE_Message FE_Alert
#endif

extern Bool FE_Confirm(MWContext * context, const char * Msg);

extern char * FE_Prompt (MWContext * context, const char * Msg, const char * dflt);

extern char * FE_PromptPassword (MWContext * context, const char * Msg);

extern char * FE_PromptMessageSubject(MWContext * context);


/* Prompt for a  username and password
 *
 * message is a prompt message.
 *
 * if username and password are not NULL they should be used
 * as default values and NOT MODIFIED.  New values should be malloc'd
 * and put in their place.  
 *
 * If the user hits cancel, FALSE should be returned; otherwise,
 * TRUE should be returned.
 */
PUBLIC Bool FE_PromptUsernameAndPassword (MWContext * window_id,
					  const char *  message,
					  char **       username,
					  char **       password);

/*
 * If the user has requested it, save the pop3 password.
 */
extern void FE_RememberPopPassword(MWContext * context, const char * password);


/* Callback for FE_PromptForFileName() and FE_PromptForNewsHost() */
typedef void (*ReadFileNameCallbackFunction) (MWContext *context,
											  char *file_name,
											  void *closure);


/* Prompt the user for a file name.
   This simply creates and raises the dialog, and returns.
   When the user hits OK or Cancel, the callback will be run.

   prompt_string: the window title, or whatever.

   default_path: the directory which should be shown to the user by default.
     This may be 0, meaning "use the same one as last time."  The pathname
	 will be in URL (Unix) syntax.  (If the FE can't do this, or if it
	 violates some guidelines, nevermind.  Unix uses it.)

   file_must_exist_p: if true, the user won't be allowed to enter the name
     of a file that doesn't exist, otherwise, they will be allowed to make
	 up new names.

   directories_allowed_p: if true, then the user will be allowed to select
     directories/folders as well; otherwise, they will be constrained to
	 select only files.

   The dialog should block the user interface while allowing
   network activity to happen.

   The callback should be called with NULL if the user hit cancel,
   and a newly-allocated string otherwise.

   The caller should be prepared for the callback to run before
   FE_PromptForFileName() returns, though normally it will be
   run some time later.

   Returns negative if something went wrong (in which case the
   callback will not be run.)
*/
extern int FE_PromptForFileName (MWContext *context,
								 const char *prompt_string,
								 const char *default_path,
								 XP_Bool file_must_exist_p,
								 XP_Bool directories_allowed_p,
								 ReadFileNameCallbackFunction fn,
								 void *closure);

/* Prompts the user for a news host, port, and protocol
   (two text fields and a check box for `news' versus `snews'.)
   Same callback conventions as FE_PromptForFileName(), except
   that the returned string is a URL of the form "http://host:port"
 */
extern int FE_PromptForNewsHost (MWContext *context,
								 const char *prompt_string,
								 ReadFileNameCallbackFunction fn,
								 void *closure);




/* FE_FileSortMethod
 * returns one of:  SORT_BY_SIZE
 *          SORT_BY_TYPE
 *          SORT_BY_DATE
 *          SORT_BY_NAME
 * this determines how files are sorted in FTP and file listings
 */
extern int FE_FileSortMethod(MWContext * window_id);

/* defines to define the sort method
 * for FTP and file listings
 *
 * these should be returned by: FE_FileSortMethod
 */
#define SORT_BY_NAME 0
#define SORT_BY_TYPE 1
#define SORT_BY_SIZE 2
#define SORT_BY_DATE 3

/* FE_UseFancyFTP: check whether or not to use fancy ftp
 */
extern Bool FE_UseFancyFTP (MWContext * window_id);

/* FE_UseFancyNewsgroupListing()
 *
 * check whether or not to use fancy newsgroup listings
 */
extern Bool FE_UseFancyNewsgroupListing(MWContext *window_id);

/* FE_ShowAllNewsArticles
 *
 * Return true if the user wants to see all newsgroup 
 * articles and not have the number restricted by
 * .newsrc entries
 */
extern XP_Bool FE_ShowAllNewsArticles(MWContext *window_id);

/*
 * Return builtin strings for about: displaying
 */
extern void* FE_AboutData(const char *which,
			 char **data_ret,
			 int32 *length_ret,
			 char **content_type_ret);

extern void FE_FreeAboutData (void * data, const char* which);


typedef enum PREF_Enum {
	PREF_EmailAddress,
	PREF_Pop3ID,
	PREF_SMTPHost,
	PREF_PopHost,
	PREF_NewsHost
} PREF_Enum ;
/* 
 * Open a preferences window for a particular preference
 */
extern void FE_EditPreference(PREF_Enum which);

/* FE_GetContextID(MWContext * window_id)
 *
 * Gets a unique id for the window
 */
extern int32 FE_GetContextID(MWContext * window_id);
#ifdef _USRDLL
extern int32 NDLLFE_GetContextID(MWContext * window_id);
#endif


/* Given a local file path (file:///) returns the MIME type of the
 * file.
 * This is needed on the Mac (and possibly on Win95, because not all
 * files have the right extension
 * fileType and encoding should be freed by the caller
 * both can be NULL on return. If they are NULL, useDefault is true
 */
#if !defined(XP_WIN) && !defined(XP_OS2)
void    FE_FileType(char * path, 
					Bool * useDefault, 
					char ** fileType, 
					char ** encoding);
#else
BOOL   FE_FileType(char * path, 
					char * fileType, 
					char * encoding);
#endif
#ifdef NEW_FE_CONTEXT_FUNCS

/* ---------------------------------------------------------------------------
 * Front end window control
 */

#define FE_CreateNewDocWindow(context, URL) \
		(*context->funcs->CreateNewDocWindow)(context, URL)

/* ---------------------------------------------------------------------------
 * Front end setup for layout
 */

#define FE_LayoutNewDocument(context, url_struct, iWidth, iHeight, mWidth, mHeight) \
			(*context->funcs->LayoutNewDocument)(context, url_struct, iWidth, iHeight, mWidth, mHeight) 
#define FE_FinishedLayout(context) \
			(*context->funcs->FinishedLayout)(context)
#define FE_SetDocTitle(context, title) \
			(*context->funcs->SetDocTitle)(context, title)

/* ---------------------------------------------------------------------------
 * Front end Information stuff
 */

#define FE_TranslateISOText(context, charset, ISO_Text) \
			(*context->funcs->TranslateISOText)(context, charset, ISO_Text)
#define FE_GetTextInfo(context, text, text_info) \
			(*context->funcs->GetTextInfo)(context, text, text_info)
#ifdef LAYERS
#define FE_GetTextFrame(context, element, start, end, frame) \
			(*context->funcs->GetTextFrame)(context, element, start, end, frame)
#endif
#define FE_GetEmbedSize(context, embed_struct, force_reload) \
			(*context->funcs->GetEmbedSize)(context, embed_struct, force_reload)
#define FE_GetJavaAppSize(context, java_struct, force_reload) \
			(*context->funcs->GetJavaAppSize)(context, java_struct, force_reload)
#define FE_GetFormElementInfo(context, form_element) \
			(*context->funcs->GetFormElementInfo)(context, form_element)
#define FE_GetFormElementValue(context, form_element,hide) \
			(*context->funcs->GetFormElementValue)(context, form_element,hide)
#define FE_ResetFormElement(context, form_element) \
			(*context->funcs->ResetFormElement)(context, form_element)
#define FE_SetFormElementToggle(context, form_element,toggle) \
			(*context->funcs->SetFormElementToggle)(context, form_element,toggle)
#define FE_FreeEmbedElement(context, data) \
			(*context->funcs->FreeEmbedElement)(context, data)
#define FE_CreateEmbedWindow(context, data) \
            (*context->funcs->CreateEmbedWindow)(context, data)
#define FE_SaveEmbedWindow(context, data) \
            (*context->funcs->SaveEmbedWindow)(context, data)
#define FE_RestoreEmbedWindow(context, data) \
            (*context->funcs->RestoreEmbedWindow)(context, data)
#define FE_DestroyEmbedWindow(context, data) \
            (*context->funcs->DestroyEmbedWindow)(context, data)
#define FE_FreeJavaAppElement(context, data) \
			(*context->funcs->FreeJavaAppElement)(context, data)
#define FE_HideJavaAppElement(context, data) \
			(*context->funcs->HideJavaAppElement)(context, data)
#define FE_FreeEdgeElement(context, data) \
			(*context->funcs->FreeEdgeElement)(context, data)
#define FE_FormTextIsSubmit(context, form_element) \
			(*context->funcs->FormTextIsSubmit)(context, form_element)


/* ---------------------------------------------------------------------------
 * Front end Drawing stuff
 */

/* defines for iLocation parameter of display functions */
#define FE_TLEDGE   0   /* Top Ledge */
#define FE_VIEW     1   /* Main View Window */
#define FE_BLEDGE   2   /* Bottom Ledge */

#define FE_DisplaySubtext(context, iLocation, text, start_pos, end_pos, need_bg) \
			(*context->funcs->DisplaySubtext)(context, iLocation, text, start_pos, end_pos, need_bg)
#define FE_DisplayText(context, iLocation, text, need_bg) \
			(*context->funcs->DisplayText)(context, iLocation, text, need_bg)
#define FE_DisplayEmbed(context, iLocation , embed_struct) \
			(*context->funcs->DisplayEmbed)(context, iLocation ,embed_struct)
#define FE_DisplayJavaApp(context, iLocation , java_struct) \
			(*context->funcs->DisplayJavaApp)(context, iLocation ,java_struct)
#define FE_DisplayEdge(context, iLocation ,edge_struct) \
			(*context->funcs->DisplayEdge)(context, iLocation ,edge_struct)
#define FE_DisplayTable(context, iLocation ,table_struct) \
			(*context->funcs->DisplayTable)(context, iLocation ,table_struct)
#define FE_DisplayCell(context, iLocation ,cell_struct) \
			(*context->funcs->DisplayCell)(context, iLocation ,cell_struct)

/* brade:  not sure why this doesn't work on Windows but seems to work elsewhere */
/*         therefore, short-term hack is to #ifdef out these lines */
#ifdef XP_MAC
#define FE_DisplayEntireTableOrCell(context, element) \
			(*context->funcs->InvalidateEntireTableOrCell)(context, element)
#define FE_DisplayAddRowOrColBorder(context, pRect, bErase) \
			(*context->funcs->DisplayAddRowOrColBorder)(context, pRect, bErase)
#define FE_UpdateEnableStates(context) \
			(*context->funcs->UpdateEnableStates)(context)
#endif

#define FE_DisplaySubDoc(context, iLocation ,subdoc_struct) \
			(*context->funcs->DisplaySubDoc)(context, iLocation ,subdoc_struct)
#define FE_DisplayLineFeed(context, iLocation , line_feed, need_bg) \
			(*context->funcs->DisplayLineFeed)(context, iLocation , line_feed, need_bg)
#define FE_DisplayHR(context, iLocation , HR_struct) \
			(*context->funcs->DisplayHR)(context, iLocation , HR_struct)
#define FE_DisplayBullet(context, iLocation, bullet) \
			(*context->funcs->DisplayBullet)(context, iLocation, bullet)
#define FE_DisplayFormElement(context, iLocation, form_element) \
			(*context->funcs->DisplayFormElement)(context, iLocation, form_element)
#define FE_DisplayBorder(context, iLocation, x, y, width, height, bw, color, style) \
            (*context->funcs->DisplayBorder)(context, iLocation, x, y, width, height, bw, color, style)
#define FE_DisplayFeedback(context, iLocation, element) \
			(*context->funcs->DisplayFeedback)(context, iLocation, element)
#ifdef LAYERS
#define FE_EraseBackground(context, iLocation, x, y, width, height, bg) \
            (*context->funcs->EraseBackground)(context, iLocation, x, y, width, height, bg)
#define FE_SetDrawable(context, drawable) \
            (*context->funcs->SetDrawable)(context, drawable)
#endif /* LAYERS */

#define FE_ClearView(context, which) \
			(*context->funcs->ClearView)(context, which)

#define FE_SetDocDimension(context, iLocation, iWidth, iLength) \
			(*context->funcs->SetDocDimension)(context, iLocation, iWidth, iLength)
#define FE_SetDocPosition(context, iLocation, iX, iY) \
			(*context->funcs->SetDocPosition)(context, iLocation, iX, iY)
#define FE_GetDocPosition(context, iLocation, iX, iY) \
			(*context->funcs->GetDocPosition)(context, iLocation, iX, iY)
#define FE_BeginPreSection(context) \
			(*context->funcs->BeginPreSection)(context)
#define FE_EndPreSection(context) \
			(*context->funcs->EndPreSection)(context)
#define FE_SetProgressBarPercent(context, percent) \
			(*context->funcs->SetProgressBarPercent)(context, percent)
#define FE_SetBackgroundColor(context, red, green, blue) \
			(*context->funcs->SetBackgroundColor)(context, red, green, blue)
#define FE_Progress(cx,msg) \
			(*cx->funcs->Progress)(cx,msg)
#define FE_SetCallNetlibAllTheTime(cx) \
			(*cx->funcs->SetCallNetlibAllTheTime)(cx)
#define FE_ClearCallNetlibAllTheTime(cx) \
			(*cx->funcs->ClearCallNetlibAllTheTime)(cx)
#define FE_GraphProgressInit(cx, URL_s, content_length) \
			(*cx->funcs->GraphProgressInit)(cx, URL_s, content_length)
#define FE_GraphProgressDestroy(cx, URL_s, content_length, total_read) \
			(*cx->funcs->GraphProgressDestroy)(cx, URL_s, content_length, total_read)
#define FE_GraphProgress(cx, URL_s, rec, new, len) \
			(*cx->funcs->GraphProgress)(cx, URL_s, rec, new, len)
#define FE_UseFancyFTP(window_id) \
			(*window_id->funcs->UseFancyFTP)(window_id)
#define FE_UseFancyNewsgroupListing(window_id) \
			(*window_id->funcs->UseFancyNewsgroupListing)(window_id)
#define FE_FileSortMethod(window_id) \
			(*window_id->funcs->FileSortMethod)(window_id)
#define FE_ShowAllNewsArticles(window_id) \
			(*window_id->funcs->ShowAllNewsArticles)(window_id)
#define FE_Confirm(context,Msg) \
			(*context->funcs->Confirm)(context,Msg)
#define FE_Prompt(context,Msg,dflt) \
			(*context->funcs->Prompt)(context,Msg,dflt)
#define FE_PromptWithCaption(context,caption,Msg,dflt) \
			(*context->funcs->PromptWithCaption)(context,caption,Msg,dflt)
#define FE_PromptPassword(context, Msg) \
			(*context->funcs->PromptPassword)(context, Msg)
#define FE_PromptUsernameAndPassword(cx, Msg, username, password) \
			(*cx->funcs->PromptUsernameAndPassword)(cx,Msg,username,password)
#define FE_EnableClicking(context) \
			(*context->funcs->EnableClicking)(context)
#define FE_GetDefaultBackgroundColor(context,color) \
			(*context->funcs->GetDefaultBackgroundColor)(context,color)

/* these functions are to allow dealyed native window applet creation and transparent applet */
#define FE_HandleClippingView(context, data, x, y, width, height) \
			(*context->funcs->HandleClippingView)(context, data, x, y, width, height)
#define FE_DrawJavaApp(context, iLocation, pJava) \
			(*context->funcs->DrawJavaApp)(context, iLocation, pJava)

/* This will be called after ALL exit routines have been called and
 * when there are no more pending or active connections in the Netlib.
 * After FE_AllConnectionsComplete(MWContext *) the context will not
 * be referenced again by the NetLib so the context can be free'd in
 * that call if desired.
 */
#define FE_AllConnectionsComplete(context) \
			(*context->funcs->AllConnectionsComplete)(context)
#else

/* ---------------------------------------------------------------------------
 * Front end window control
 */

extern MWContext *  FE_CreateNewDocWindow(MWContext * calling_context,URL_Struct * URL);

/* ---------------------------------------------------------------------------
 * Front end setup for layout
 */

extern void FE_LayoutNewDocument(MWContext *context, URL_Struct *url_struct, int32 *iWidth, int32 *iHeight, int32 *mWidth, int32 *mHeight);
extern void FE_SetDocTitle(MWContext * context, char * title);
extern void FE_FinishedLayout (MWContext *context);
extern void FE_BeginPreSection (MWContext *context);
extern void FE_EndPreSection (MWContext *context);

/* ---------------------------------------------------------------------------
 * Front end Information stuff
 */

extern char *   FE_TranslateISOText(MWContext * context, int charset, char *ISO_Text);
extern int      FE_GetTextInfo(MWContext * context, LO_TextStruct *text, LO_TextInfo *text_info);
#ifdef XP_OS2       /* performance   */
extern int      FE_GetMaxWidth(MWContext *pContext, LO_TextStruct *pText); /* performance */
#endif
#ifdef LAYERS
void		FE_GetTextFrame(MWContext * context, LO_TextStruct *text, int32 start, int32 end, XP_Rect *frame);
#endif
void            FE_GetEmbedSize(MWContext * context, LO_EmbedStruct *embed_struct, NET_ReloadMethod force_reload);
void            FE_GetJavaAppSize(MWContext * context, LO_JavaAppStruct *java_struct, NET_ReloadMethod force_reload);
void            FE_GetFormElementInfo(MWContext * context, LO_FormElementStruct * form_element);
void            FE_GetFormElementValue(MWContext * context, LO_FormElementStruct * form_element, Bool hide);
void            FE_ResetFormElement(MWContext * context, LO_FormElementStruct * form_element);
void            FE_SetFormElementToggle(MWContext * context, LO_FormElementStruct * form_element, Bool toggle);
void            FE_FreeEmbedElement(MWContext *context, LO_EmbedStruct *);
void            FE_FreeJavaAppElement(MWContext *context, struct LJAppletData *appletData);
void            FE_HideJavaAppElement(MWContext *context, void*);
void            FE_FreeEdgeElement(MWContext *context, LO_EdgeStruct *);
void            FE_FormTextIsSubmit(MWContext * context, LO_FormElementStruct * form_element);
void            FE_SetProgressBarPercent(MWContext *context, int32 percent);
void            FE_SetBackgroundColor(MWContext *context, uint8 red, uint8 green, uint8 blue);


/* ---------------------------------------------------------------------------
 * Front end Drawing stuff
 */

/* defines for iLocation parameter of display functions */
#define FE_TLEDGE   0   /* Top Ledge */
#define FE_VIEW     1   /* Main View Window */
#define FE_BLEDGE   2   /* Bottom Ledge */

extern void FE_DisplaySubtext(MWContext * context, int iLocation, LO_TextStruct *text, int32 start_pos, int32 end_pos, Bool need_bg);
extern void FE_DisplayText(MWContext * context, int iLocation, LO_TextStruct *text, Bool need_bg);
void        FE_DisplayEmbed(MWContext * context, int iLocation ,LO_EmbedStruct *embed_struct);
void        FE_DisplayJavaApp(MWContext * context, int iLocation ,LO_JavaAppStruct *java_struct);
void        FE_DisplayEdge(MWContext * context, int iLocation ,LO_EdgeStruct *edge_struct);
void        FE_DisplayTable(MWContext * context, int iLocation ,LO_TableStruct *table_struct);
void        FE_DisplayCell(MWContext * context, int iLocation ,LO_CellStruct *cell_struct);
void        FE_DisplaySubDoc(MWContext * context, int iLocation ,LO_SubDocStruct *subdoc_struct);
void        FE_DisplayLineFeed(MWContext * context, int iLocation , LO_LinefeedStruct *line_feed, Bool need_bg);
void        FE_DisplayHR(MWContext * context, int iLocation , LO_HorizRuleStruct *HR_struct);
void        FE_DisplayBullet(MWContext *context, int iLocation, LO_BullettStruct *bullet);
void        FE_DisplayFormElement(MWContext * context, int iLocation, LO_FormElementStruct * form_element);
void        FE_DisplayBorder(MWContext *context, int iLocation, int x, int y, int width, int height, int bw, LO_Color *color, LO_LineStyle style);
void        FE_DisplayFeedback(MWContext *context, int iLocation, LO_Element *element);
#ifdef LAYERS
void        FE_EraseBackground(MWContext * context, int iLocation, int32 x, int32 y, uint32 width, uint32 height, LO_Color *bg);
void        FE_SetClipRegion(MWContext *context, int iLocation, FE_Region region);
#endif
void        FE_ClearView(MWContext * context, int which);

void        FE_SetDocDimension(MWContext *context, int iLocation, int32 iWidth, int32 iLength);
void        FE_SetDocPosition(MWContext *context, int iLocation, int32 iX, int32 iY);
void        FE_GetDocPosition(MWContext *context, int iLocation, int32 *iX, int32 *iY);

/* temporary testing fn's */

extern int  FE_DrawText(MWContext * context, const char * buffer);
extern int  FE_DrawImage(MWContext * context, const char * filename);

extern void FE_EnableClicking(MWContext * context);

/* these functions are to allow dealyed native window applet creation and transparent applet */
void        FE_HandleClippingView(MWContext *pContext, struct LJAppletData *appletD, int x, int y, int width, int height);
void        FE_DrawJavaApp(MWContext *pContext, int iLocation, LO_JavaAppStruct *pJava);

/* This will be called after ALL exit routines have been called and
 * when there are no more pending or active connections in the Netlib.
 * After FE_AllConnectionsComplete(MWContext *) the context will not
 * be referenced again by the NetLib so the context can be free'd in
 * that call if desired.
 */
extern void FE_AllConnectionsComplete(MWContext * context);

#endif /* NEW_FE_CONTEXT_FUNCS */

/*
 * Logically grouped with FE_GetTextInfo
 */
extern void     FE_ReleaseTextAttrFeData(MWContext * context, LO_TextAttr *attr);

/*
 * Liberated from context function table
 */
void            FE_FreeFormElement(MWContext * context, LO_FormElementData *);

/* --------------------------------------------------------------------------
 * Front end history stuff
 */

extern int  FE_EnableBackButton(MWContext * context);
extern int  FE_EnableForwardButton(MWContext * context);
extern int  FE_DisableBackButton(MWContext * context);
extern int  FE_DisableForwardButton(MWContext * context);

/* -------------------------------------------------------------------------
 * Generic FE stuff
 */
/* display given string in 'view source' window */
extern void FE_DisplaySource(MWContext * context, char * source);
extern void FE_SaveAs(MWContext * context, char * source);

/* This is called when there's a chance that the state of the
 * Stop button (and corresponding menu item) has changed.
 * The FE should use XP_IsContextStoppable to determine the
 * new state.
 */
extern void FE_UpdateStopState(MWContext * context);

/* -------------------------------------------------------------------------
 * Grid stuff (where should this go?)
 */
extern MWContext *FE_MakeGridWindow (MWContext *old_context,
	void *hist_list, void *history,
	int32 x, int32 y, int32 width, int32 height,
	char *url_str, char *window_name, int8 scrolling,
	NET_ReloadMethod force_reload
    , Bool no_edge
    );
extern void *FE_FreeGridWindow(MWContext *context, XP_Bool save_history);
extern void FE_RestructureGridWindow(MWContext *context, int32 x, int32 y,
	int32 width, int32 height);
extern void FE_GetFullWindowSize(MWContext *context,
	int32 *width, int32 *height);
extern void FE_GetEdgeMinSize(MWContext *context, int32 *size
#if defined(XP_WIN) || defined(XP_OS2)
			      /* Windows needs this info here */
			      , Bool no_edge
#endif
			      );
extern void FE_LoadGridCellFromHistory(MWContext *context, void *hist,
			NET_ReloadMethod force_reload);

/*
 * Ugh for scrolling chat window.  And now for JavaScript
 */
extern void FE_ShiftImage (MWContext *context, LO_ImageStruct *lo_image);
extern void FE_ScrollDocTo (MWContext *context, int iLocation, int32 x,int32 y);
extern void FE_ScrollDocBy (MWContext *context, int iLocation, int32 x,int32 y);

/* JavaScript commands to allow full UI control of a window */
extern void FE_BackCommand (MWContext *context);
extern void FE_ForwardCommand (MWContext *context);
extern void FE_HomeCommand (MWContext *context);
extern void FE_PrintCommand (MWContext *context);
/* Note: on Unix, the position reported by FE_GetWindowOffset() may get out 
 *  of sync with reality, because we're relying on incoming events to tell
 *  us the offset, rather than have to make a round trip to the X server
 *  every time we need it.  If it causes a problem, come talk to francis.
 *
 * For more details, see FE_GetWindowOffset() in ns/cmd/xfe/context_func.cpp.
 */
extern void FE_GetWindowOffset (MWContext *pContext, int32 *sx, int32 *sy);
extern void FE_GetScreenSize (MWContext *pContext, int32 *sx, int32 *sy);
extern void FE_GetAvailScreenRect (MWContext *pContext, int32 *sx, int32 *sy, 
							int32 *left, int32 *top);
extern XP_Bool FE_FindCommand (MWContext *context, char *szName, XP_Bool bCaseSensitve, 
							   XP_Bool bBackwards, XP_Bool bWrap);
extern void FE_GetPixelAndColorDepth (MWContext *pContext, int32 *pixelDepth, 
									  int32 *colorDepth);
/*
 *  To get information about the hardware
 */
/*
 *  (need to add functions for other platforms!!!)
 */
#if defined(XP_WIN)
extern char *FE_SystemCPUInfo(void);
extern int32 FE_SystemRAM( void );
extern int32 FE_SystemClockSpeed( void );
#endif
/*
 * Named windows needs this.
 */
extern MWContext *FE_MakeBlankWindow(MWContext *old_context,
	URL_Struct *url, char *window_name);
extern void FE_SetWindowLoading(MWContext *context, URL_Struct *url,
	Net_GetUrlExitFunc **exit_func);

/*
 * Raise the window to the top of the view order
 */
extern void FE_RaiseWindow(MWContext *context);

/* Chrome controlled windows. */
/* FE_MakeNewWindow()
 *
 * if Chrome is NULL, the behaviour will exactly be as FE_MakeBlankWindow
 */
extern MWContext *FE_MakeNewWindow(MWContext *old_context,
	URL_Struct *url, char *window_name, Chrome *chrome);
extern void FE_DestroyWindow(MWContext *context);

#if defined(XP_WIN) || defined(XP_MAC) || defined(XP_OS2)
/* -------------------------------------------------------------------------
 * FE Remote Control APIs called by netlib
 */
XP_Bool FE_UseExternalProtocolModule(MWContext *pContext,
	FO_Present_Types iFormatOut, URL_Struct *pURL,
	Net_GetUrlExitFunc *pExitFunc);

#ifdef _USRDLL
XP_Bool NDLLFE_UseExternalProtocolModule(MWContext *pContext,
	FO_Present_Types iFormatOut, URL_Struct *pURL,
	Net_GetUrlExitFunc *pExitFunc);
#endif

void FE_URLEcho(URL_Struct *pURL, int iStatus, MWContext *pContext);
#ifdef _USRDLL
void NDLLFE_URLEcho(URL_Struct *pURL, int iStatus, MWContext *pContext);
#endif

#endif /* XP_WIN */

PUBLIC void FE_GetDocAndWindowPosition(MWContext * context, int32 *pX, int32 *pY,
		int32 *pWidth, int32 *pHeight );

#ifdef EDITOR
PUBLIC void FE_DisplayTextCaret(MWContext * context, int loc, LO_TextStruct * text_data, int char_offset);

PUBLIC void FE_DisplayImageCaret(MWContext * context, LO_ImageStruct * pImageData,
			ED_CaretObjectPosition caretPos );

PUBLIC void FE_DisplayGenericCaret(MWContext * context, LO_Any * pLoAny,
			ED_CaretObjectPosition caretPos );

#ifdef XP_WIN
/* Ask front end to show where a dragged set of table cells may be dropped */
PUBLIC void FE_DisplayDropTableFeedback(MWContext * context, EDT_DragTableData *pDragData);
#endif

PUBLIC Bool FE_GetCaretPosition(MWContext *context, LO_Position* where,
    int32* caretX, int32* caretYLow, int32* caretYHigh);

PUBLIC void FE_DestroyCaret(MWContext *pContext);
PUBLIC void FE_ShowCaret(MWContext *pContext);
PUBLIC void FE_DocumentChanged(MWContext * context, int32 iStartY, int32 iHeight );

PUBLIC MWContext *FE_CreateNewEditWindow(MWContext *pContext, URL_Struct *pURL);

/* Set default colors, background from user Preferences via the Page Data structure 
*/
void FE_SetNewDocumentProperties(MWContext * pMWContext);

/*
 * Formatting has changed.
 */
PUBLIC void FE_EditFormattingUpdate( MWContext *pContext );

/* 
 * Brings up a modal image load dialog and returns.  Calls
 *  EDT_ImageLoadCancel() if the cancel button is pressed
*/
PUBLIC void FE_ImageLoadDialog( MWContext *pContext );

/* 
 * called by the editor engine after the image has been loaded
*/
PUBLIC void FE_ImageLoadDialogDestroy( MWContext *pContext );

/* 
 * Bring up a files saving progress dialog.  While the dialog is modal to the edit
 *  window. but should return immediately from the create call.  If cancel button
 *  is pressed, EDT_SaveCancel(pMWContext) should be called if saving locally,
 *  or  NET_InterruptWindow(pMWContext) if uploading files
 *  Use bUpload = TRUE for Publishing file and images to remote site
*/
PUBLIC void FE_SaveDialogCreate( MWContext *pContext, int iFileCount, ED_SaveDialogType saveType );
/* Sent after filename is made, but before opening or saving */
PUBLIC void FE_SaveDialogSetFilename( MWContext *pContext, char *pFilename );
					 
/* Sent after file is completely saved, even if user said no to overwrite
 *  (then status = ED_ERROR_FILE_EXISTS)
 * Not called if failed to open source file at all, but is called with error
 *   status if failed during writing
*/
PUBLIC void FE_FinishedSave( MWContext *pMWContext, int status, char *pDestURL, int iFileNumber );
/* Sent after all files are saved or user cancels */
PUBLIC void FE_SaveDialogDestroy( MWContext *pContext, int status, char *pFilename );

PUBLIC ED_SaveOption FE_SaveFileExistsDialog( MWContext *pContext, char* pFilename );

/* Sent after file is opened (or failed) -- same time saving is initiated */
PUBLIC Bool FE_SaveErrorContinueDialog( MWContext *pContext, char* pFileName, ED_FileError error );

PUBLIC void FE_ClearBackgroundImage( MWContext *pContext );

PUBLIC void FE_EditorDocumentLoaded( MWContext *pContext );

/* 
 * return a valid local base file name for the given URL.  If there is no
 *  base name, return 0.
*/
PUBLIC char* FE_URLToLocalName( char* );

PUBLIC Bool FE_EditorPrefConvertFileCaseOnWrite(void);

/* Defines to use for second parameter to FE_LoadUrl 
 * for better readability
*/
#define LOAD_URL_COMPOSER  TRUE
#define LOAD_URL_NAVIGATOR FALSE

/* Access function for calling from outside to Edit or Navigate
 * Primarily called by LiveWire SiteManager 
 * Set bEdit = TRUE to start Editor
 * Existing frames are searched first and activated if 
 *   URL is already loaded
 * Nothing happens if szURL is NULL
*/
void FE_LoadUrl( char *szURL, XP_Bool bEdit );

/* This is used to find an existing Browser or Editor frame
 *  and load supplied URL. It is important to prevent 
 *  multiple windows editing the same file 
*/
MWContext * FE_ActivateFrameWithURL(char *szURL, Bool bFindEditor);

/* Primarily an error recovery routine:
 * Try to find frame with our URL,
 *   or the previous frame in list,
 *   or create a new browser frame
 *   Then close current frame
 *   Return frame found or created
*/
void FE_RevertToPreviousFrame(char *szURL, MWContext *pMWContext);

/*   Does editor-specific stuff when done loading a URL
*/
void FE_EditorGetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pMWContext);

/* Check if change was made to an edit document, 
 *    prompt user to save if they was.
 * Returns TRUE for all cases except CANCEL by the user in any dialog
 * Call this before any URL load into the current frame window
*/
Bool FE_CheckAndSaveDocument(MWContext *pMWContext);

/* Similar to above, but dialog to ask user if they want to save changes
 *    should have "AutoSave" caption and extra line to
 *    tell them "Cancel" will turn off Autosave until they 
 *    save the file later.
*/
Bool FE_CheckAndAutoSaveDocument(MWContext *pMWContext);

/* Checks for new doc or remote editing and prompts
 *  to save. Return FALSE only if user cancels out of dialog
 * Use bSaveNewDocument = FALSE to not force saving of a new document
*/
Bool FE_SaveNonLocalDocument(MWContext *pMWContext, Bool  bSaveNewDocument);

#if defined(XP_WIN) || defined(XP_OS2)
/* Tell the front end every time we finish relaying out so
 *  it can invalidate cached pointers to layout elements
*/
void FE_FinishedRelayout(MWContext * pMWContext);

/* Call the front end to force updating button enable/disabling
 * Currently used after every Autosave event so toolbar SAVE button can be grayed 
*/
void FE_UpdateEnableStates(MWContext * pMWContext);
#endif /* XP_WIN */

#if defined(XP_WIN) || defined(XP_UNIX)
/* MOVE THESE OUT OF XP_WIN WHEN ALL PLATFORMS IMPLEMENT IT*/
/* Called during "sizing" of a table when we are really adding/subtracting
 * rows or columns. Front end should draw a line
 *   from the point {rect.left, rect.top} to the point {rect.right,rect.bottom}
 *   (Thickness and style are left to FE, Windows uses a dashed line)
 *   This will be called with bErase = TRUE to remove the line when appropriate,
 *   so FEs don't need to worry about removing selection,
 *   but you must call EDT_CancelSizing or EDT_EndSizing for final removal.
*/
void FE_DisplayAddRowOrColBorder(MWContext * pMWContext, XP_Rect *pRect, XP_Bool bErase);

/* This should invalidate the entire table or cell rect to cause redrawing of backgrounds
 *  and all table or cell contents. Use to unselect table/cell selection feedback
*/
void FE_DisplayEntireTableOrCell(MWContext * pMWContext, LO_Element * pLoElement);

#endif /* XP_WIN || XP_UNIX */

#endif /* Editor */

/* This is how the libraries ask the front end to load a URL just
   as if the user had typed/clicked it (thermo and busy-cursor
   and everything.)  NET_GetURL doesn't do all this stuff.
 */
extern int FE_GetURL (MWContext *context, URL_Struct *url);

/* -------------------------------------------------------------------------
 * Input focus and event controls, for Mocha.
 */

/*
 * Force input to be focused on an element in the given window.
 */
void FE_FocusInputElement(MWContext *window, LO_Element *element);

/*
 * Force input to be defocused from an element in the given window.
 * It's ok if the input didn't have input focus.
 */
void FE_BlurInputElement(MWContext *window, LO_Element *element);

/*
 * Selectin input in a field, highlighting it and preparing for change.
 */
void FE_SelectInputElement(MWContext *window, LO_Element *element);

/*
 * Tell the FE that something in element changed, and to redraw it in a way
 * that reflects the change.  The FE is responsible for telling layout about
 * radiobutton state.
 */
void FE_ChangeInputElement(MWContext *window, LO_Element *element);

/*
 * Tell the FE that a form is being submitted without a UI gesture indicating
 * that fact, i.e., in a Mocha-automated fashion ("document.form.submit()").
 * The FE is responsible for emulating whatever happens when the user hits the
 * submit button, or auto-submits by typing Enter in a single-field form.
 */
void FE_SubmitInputElement(MWContext *window, LO_Element *element);

/*
 * Emulate a button or HREF anchor click for element.
 */
/*
#ifndef NO_TAB_NAVIGATION
	FE_ClickAnyElement() is based on FE_ClickInputElement(), and should replace the latter.
	If haveXY != 0, the xx, yy will be used for click. otherwise click the center of element.

	xx, yy are element coordinates, This function take care of :
	lo_any.x_offset, pWinCX->GetOriginX(), ClientToScreen(), and  ScreenToClient()
*/
int FE_ClickAnyElement(MWContext * pContext,  LO_Element * pElement, int haveXY, int32 xx, int32 yy );
/* NO_TAB_NAVIGATION  */


void FE_ClickInputElement(MWContext *window, LO_Element *element);


char *FE_GetAcceptLanguage(void);

#ifdef LAYERS
PRBool FE_HandleLayerEvent(MWContext *context, CL_Layer *layer, 
                           CL_Event *event);
PRBool FE_HandleEmbedEvent(MWContext *context, LO_EmbedStruct *embed,
                           CL_Event *event);
#endif /* LAYERS */

/*
 * Configurable chrome for existing contexts.
 *
 * This routine uses only these entries from the chrome structure.
 *      show_url_bar, show_button_bar, show_directory_buttons,
 *      show_security_bar, show_menu, show_bottom_status_bar
 */
void FE_UpdateChrome(MWContext *window, Chrome *chrome);

/*
 * JavaScript calls to the window must be able to find the state of 
 * the existing chrome structure
 */
void FE_QueryChrome(MWContext *window, Chrome *chrome);

/*
 * Returns a string representing the default installed location of
 * the NetHelp directory as a file:// URL.  The caller is expected to
 * free the returned string.  Used by mkhelp.c to retrieve NetHelp content.
 */

extern char * FE_GetNetHelpDir(void);

/*
 * Returns the "default" MWContext to use when bringing up a nethelp
 * topic.  This should try to return the context associated with the topmost
 * visible window.
 */

extern MWContext * FE_GetNetHelpContext(void);

/*
 * Netcaster calls - EA
 */

/*
 * Returns true if Netcaster is installed, false otherwise.
 */

extern XP_Bool FE_IsNetcasterInstalled(void);

/*
 * If Netcaster is running, returns its context.  If not, returns NULL.
 * This function will always return NULL if Netcaster is not installed.
 */

extern MWContext * FE_IsNetcasterRunning(void);
/*
 * If the Netcaster component is not currently running, starts it and opens
 * the Netcaster drawer.  If the component is running, but it does not have
 * focus (it's not on top), or the drawer is closed, brings the window to top
 * and opens the drawer.  If the component is running, has focus, and is open,
 * does nothing.
 */

extern void FE_RunNetcaster(MWContext *context);


#if defined(XP_WIN) || defined(XP_OS2) /* removed this #ifdef when other platform implement this. */
extern XP_Bool FE_CheckFormTextAttributes(MWContext *context,
                LO_TextAttr *oldAttr, LO_TextAttr *newAttr, int32 type);
#endif

#if defined(XP_WIN) || defined(XP_OS2)
/*
 *      Windows needs a way to know when a URL switches contexts,
 *              so that it can keep the NCAPI progress messages
 *              specific to a URL loading and not specific to the
 *              context attempting to load.
 */
void FE_UrlChangedContext(URL_Struct *pUrl, MWContext *pOldContext, MWContext *pNewContext);

/* these are used to load and use the compuserv auth DLL
 */
extern int WFE_DoCompuserveAuthenticate(MWContext *context, 
										URL_Struct *URL_s, 
										char *authenticate_header_value);

extern char *WFE_BuildCompuserveAuthString(URL_Struct *URL_s);

/* Way to attempt to keep the application messages flowing 
 * when need to block a return value.
 */
extern void FEU_StayingAlive(void);

/* convert logical pixels to device pixels */
extern int32  FE_LPtoDPoint(MWContext * context, int32 logicalPoint);

#endif /* XP_WIN */


extern void	FE_MochaImageGroupObserver(XP_Observable observable,
										XP_ObservableMsg message,
										void *message_data,
										void *closure);

/*
 * Get a context at init time suitable for putting up a password prompt.
 * We can add other features as necessary. This context is only needed before
 * we get the first window up. This function is need if FIPS140 is turned on.
 */
MWContext * FE_GetInitContext(void);

#ifdef XP_UNIX
/* Get the dimensions of an icon in pixels for the PostScript front end. */
extern void
FE_GetPSIconDimensions(int icon_number, int *width, int *height);

/* Fill in the bits of an icon for the PostScript front end. */
extern XP_Bool
FE_GetPSIconData(int icon_number, IL_Pixmap *image, IL_Pixmap *mask);
#endif

#ifdef XP_WIN
/* Check if Netscape is registered as default browser */
PRBool FE_IsNetscapeDefault(void);

/* Tell front end to set Netscape as default browser (if applicable on platform) */
PRBool FE_MakeNetscapeDefault(void);
#endif

XP_END_PROTOS

#endif /* _FrontEnd_ */
