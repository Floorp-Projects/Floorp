/* $Id: QtContext.cpp,v 1.1 1998/09/25 18:01:28 ramiro%netscape.com Exp $
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

#include <qtimer.h>
#include <qapp.h>
#include "QtBrowserContext.h" // includes QtContext.h
#include "qtbind.h"
#include <qmsgbox.h>
#include "layers.h"
#include "DialogPool.h"
#include "shist.h"
#include "SaveAsDialog.h"
#include "streaming.h"
//#include "QtHistoryContext.h"
#include <qobjcoll.h>
#include <string.h>
#include <qstring.h>
#include <qpainter.h>
#include <qmainwindow.h>
#include <qstatusbar.h>
#include <qapp.h>

#include "libimg.h"             /* Image Library public API. */


extern void fe_StartProgressGraph ( MWContext* context );
extern void fe_SetCursor ( MWContext* context, bool );
extern void fe_url_exit (URL_Struct *url, int status, MWContext *context);

QtContext::QtContext( MWContext* c ) :
    internal_painter(0),
    context(c)
{
    dialogpool = new DialogPool();

    context->compositor = 0; // not made yet.

    // This must be done, before the Session History Manager is
    // constructed because it will call back FE_* methods which crash
    // when not registered.
    // This is a H*** , but Matthias forbade me to use the H-word, so I
    // am calling this "not so clean code" for now.
#if defined(XP_UNIX)
    c->fe.data = (fe_ContextData*)this;
#elif defined(XP_WIN)
    c->fe.cx = (CAbstractCX*)this;
#endif
    internal_x_origin = internal_y_origin = 0;

    // Initialize Session History library
    SHIST_InitSession( c );
    dont_free_context_memory = false;
    synchronous_url_dialog = 0;
    synchronous_url_exit_status = 0;
    delete_response = 0;
    destroyed = false;
    looping_images_p = false;     /* TRUE if images are looping. */
    active_url_count = 0;
    clicking_blocked = false;
    have_tried_progress = false;
    bw = 0;
    scrolling_policy = LO_SCROLL_AUTO;
}

QtContext::~QtContext()
{
    clearPainter();

    // Uninitialize Session History library
    SHIST_EndSession( context );

    delete dialogpool;

    //XP_InterruptContext (context);

#if defined(XP_UNIX)
    mwContext()->fe.data = (fe_ContextData*)0;
#elif defined(XP_WIN)
    mwContext()->fe.cx = (CAbstractCX*)0;
#endif

    XP_RemoveContextFromList(mwContext());
}

static void useArgs( const char *fn, ... )
{
    //remove 0&& to enable debug printouts
    if (0&&fn) printf( "%s\n", fn );
}

/* From ./dialogs.c: */
/* puts up a FE security dialog
 *
 * Should return TRUE if the url should continue to
 * be loaded
 * Should return FALSE if the url should be aborted
 */
bool QtContext::securityDialog(int state, char *prefs_toggle)
{
    useArgs("FE_SecurityDialog(%d, %p)", state, prefs_toggle);
    return FALSE;
}

/* From ./prefdialogs.c: */
/*
 * Inform the FE that the security library is or is not using a password.
 *      "cx" is the window context
 *      "usePW" is true for "use a password", false otherwise
 */
void QtContext::setPasswordEnabled(bool usePW)
{
    useArgs("FE_SetPasswordEnabled(%d)", usePW);
}

/* Header file comment: */
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
/* From ./prefdialogs.c: */
void QtContext::setPasswordAskPrefs(int askPW, int timeout)
{
    useArgs("FE_SetPasswordAskPrefs(%d, %d)", askPW, timeout);
}

/* From ./xfe.c: */
/* Cipher preference handling:
 *
 * Sets the cipher preference item in the FE and saves the preference file.
 * The FE will take a copy of passed in cipher argument.
 * If context is NULL, FE will choose an appropriate context if neccessary.
 */
void QtContext::setCipherPrefs(char *cipher)
{
    useArgs("FE_SetCipherPrefs(%s)", cipher);
}

/* From ./mkanim.c: */
void QtContext::alert(const char *message)
{
    //    debug("FE_Alert(%s)", message);
    QMessageBox::critical(0, tr( "QtMozilla Alert" ), message, tr( "Oops!" ) );
}


bool QtContext::confirm( QtContext* cx, const char* message )
{
    return QMessageBox::information( cx ? cx->topLevelWidget() : 0,
				     "QtMozilla Question",
				     message,
				     "Continue", "Abort", 0, 0, 1 ) == 0;
}

bool QtContext::confirm( const char* message )
{
    return confirm(this, message);
}

/* From ./dialogs.c: */
void QtContext::message(const char* message)
{
    useArgs("FE_Message(%s)", message);
}

/* From ./src/context_funcs.cpp: */
char * QtContext::promptMessageSubject()
{
    useArgs("FE_PromptMessageSubject()");
    return "from FE_PromptMessageSubject";
}

/* From ./src/context_funcs.cpp: */
/*
 * If the user has requested it, save the pop3 password.
 */
void QtContext::rememberPopPassword(const char *password)
{
    useArgs("FE_RememberPopPassword(%s)", password);
}

/* Header file comment: */
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
/* From ./src/context_funcs.cpp: */
int QtContext::promptForFileName(const char *prompt_string,
		      const char *default_path,
		      bool file_must_exist_p,
		      bool directories_allowed_p,
		      ReadFileNameCallbackFunction fn,
		      void *closure)
{
   char* file;
   QFileDialog diag(default_path, 0, 0, 0, TRUE);
   diag.setCaption(prompt_string);
   if (diag.exec()==QDialog::Accepted && diag.selectedFile()!=0)
   {
      file = strdup(diag.selectedFile());
      fn(mwContext(), file, closure);
   }
   return 0;
}

/* From ./src/context_funcs.cpp: */
char * QtContext::prompt(const char *message, const char *deflt)
{
    useArgs("QTFE_Prompt(%s, %s)", message, deflt);
    return 0;
}

/* From ./src/context_funcs.cpp: */
char * QtContext::promptWithCaption(const char *caption,
		      const char *message, const char *deflt)
{
    useArgs("QTFE_PromptWithCaption(%s, %s, %s)", caption, message, deflt);
    return 0;
}

/* From ./dialogs.c: */
char * QtContext::promptPassword(const char *message)
{
    useArgs( "QtContext::promptPassword %s", message);
    return 0;
}

void
url_exit(URL_Struct *url, int status, MWContext *context)
{
  if ( context )
    QtContext::qt(context)->urlDone( url, status );

  if ( status != MK_CHANGING_CONTEXT )
    NET_FreeURLStruct (url);

  if (QtContext::qt(context)->destroyed){
    XP_RemoveContextFromList(context);
    delete QtContext::qt(context);
  }
}


/* From ./xfe.c: */
MWContext * QtContext::makeNewWindow(URL_Struct *url,
		 char       *window_name,
		 Chrome     *chrome,
		 MWContext  *parent){
    MWContext* context = XP_NewContext();
    context->type = chrome ? chrome->type
		      : parent ? parent->type
		          : MWContextBrowser;
    //QWidget* parentw = parent ? qt(parent)->contentWidget() : 0;
    QWidget* parentw = 0;
    context->funcs = &qtbind;
    QtContext *qc=0;
    XP_AddContextToList (context);
    switch (context->type) {
      case MWContextBrowser:    
         qc = new QtBrowserContext(context, chrome, parentw, window_name);
      // Make sure the browser is visible before blocking call to
      // getURL is called below:
                             qApp->syncX();
                             qApp->processEvents();
                             break;
       case MWContextDialog:
          qc = new QtBrowserContext(context, chrome, parentw, window_name); 
          break;
#if 0
       case MWContextHistory:		
          qc = new QtHistoryContext(context); 
          break;
#endif
#if 1
	default:
	    fatal("This is here so you don't make one accidentally");
#else
      case MWContextMail:		qc = new QtMailContext(context); break;
      case MWContextNews:		qc = new QtNewsContext(context); break;
      case MWContextMailMsg:		qc = new QtMailMsgContext(context); break;
      case MWContextNewsMsg:		qc = new QtNewsMsgContext(context); break;
      case MWContextMessageComposition:	qc = new QtMessageCompositionContext(context); break;
      case MWContextSaveToDisk:		qc = new QtSaveToDiskContext(context); break;
      case MWContextText:		qc = new QtTextContext(context); break;
      case MWContextPostScript:		qc = new QtPostScriptContext(context); break;
      case MWContextBiff:		qc = new QtBiffContext(context); break;
      case MWContextEditor:		qc = new QtEditorContext(context); break;
      case MWContextJava:		qc = new QtJavaContext(context); break;
      case MWContextBookmarks:		qc = new QtBookmarksContext(context); break;
      case MWContextAddressBook:	qc = new QtAddressBookContext(context); break;
      case MWContextOleNetwork:		qc = new QtOleNetworkContext(context); break;
      case MWContextPrint:		qc = new QtPrintContext(context); break;
      case MWContextMetaFile:		qc = new QtMetaFileContext(context); break;
      case MWContextSearch:		qc = new QtSearchContext(context); break;
      case MWContextSearchLdap:		qc = new QtSearchLdapContext(context); break;
      case MWContextHTMLHelp:		qc = new QtHTMLHelpContext(context); break;
      case MWContextMailFilters:	qc = new QtMailFiltersContext(context); break;
      case MWContextMailNewsProgress:	qc = new QtMailNewsProgressContext(context); break;
      case MWContextPane:		qc = new QtPaneContext(context); break;
      case MWContextRDFSlave:		qc = new QtRDFSlaveContext(context); break;
      case MWContextProgressModule:	qc = new QtProgressModuleContext(context); break;
      case MWContextIcon:		qc = new QtIconContext(context); break;
       default:
          fatal("This is here so you don't make one accidentally");
#endif
    }
#if defined(XP_UNIX)
    context->fe.data = (fe_ContextData*)qc;
#elif defined(XP_WIN)
    context->fe.cx = (CAbstractCX*)qc;
#endif

    context->compositor = qc->createCompositor(context);

    if (url)
      qc->getURL(url);

    // ### It might be safer if we called the widget show() here, rather
    // ### than expecting the qc to do it during construction.  If we
    // ### get occasional blank new windows, do it.

    return context;
}



extern "C" {
  void fe_set_drawable_origin(CL_Drawable *drawable, int32 x_origin, int32 y_origin) ;
  void fe_set_drawable_clip(CL_Drawable *drawable, FE_Region clip_region);
  void fe_restore_drawable_clip(CL_Drawable *drawable);
  void fe_copy_pixels(CL_Drawable *drawable_src,
		      CL_Drawable *drawable_dst,
		      FE_Region region);

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
    NULL,
    NULL
};

#ifdef QTFE_DOUBLE_BUFFER_IMPLEMENTED
static
CL_DrawableVTable bs_drawable_vtable = {
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
#endif


/* Callback to set the XY offset for all drawing into this drawable */
static void
fe_set_drawable_origin(CL_Drawable *drawable, int32 x_origin, int32 y_origin)
{
  ((QtContext*)CL_GetDrawableClientData(drawable))->
	setDrawableOrigin(x_origin, y_origin);
}

void QtContext::setDrawableOrigin(int  x_origin, int y_origin){
  internal_x_origin = x_origin;
  internal_y_origin = y_origin;
}

/* Callback to set the clip region for all drawing calls */
static void
fe_set_drawable_clip(CL_Drawable *drawable, FE_Region clip_region)
{
    if ( clip_region )
	((QtContext*)CL_GetDrawableClientData(drawable))->
	    painter()->setClipRegion(*((QRegion*)clip_region));
    else
	((QtContext*)CL_GetDrawableClientData(drawable))->
	    painter()->setClipping(FALSE);
}

/* Callback not necessary, but may help to locate errors */
static void
fe_restore_drawable_clip(CL_Drawable *drawable)
{
    ((QtContext*)CL_GetDrawableClientData(drawable))->
	painter()->setClipping(FALSE);
}

#ifdef QTFE_DOUBLE_BUFFER_IMPLEMENTED
static void
fe_copy_pixels(CL_Drawable *drawable_src,
               CL_Drawable *drawable_dst,
               FE_Region region)
{
  useArgs("fe_copy_pixels %p %p %p",drawable_src,drawable_dst,region);
}
#endif


void QtContext::adjustCompositorSize()
{
    if (context->compositor) {
        int comp_width = visibleWidth();
        int comp_height = visibleHeight();
        CL_ResizeCompositorWindow(context->compositor, comp_width, comp_height);
    }
}

void QtContext::adjustCompositorPosition()
{
    if (context->compositor) {
        int x = documentXOffset();
        int y = documentYOffset();
        CL_ScrollCompositorWindow(context->compositor, x, y);
    }
}


CL_Compositor *
QtContext::createCompositor (MWContext *context)
{
  if (!contentWidget()){
    return 0;
  }

  int comp_width = visibleWidth();
  int comp_height = visibleHeight();

  CL_Drawable *cl_window_drawable;


  /* Create XP handle to window's HTML view for compositor */
  cl_window_drawable = CL_NewDrawable(comp_width, comp_height,
				      CL_WINDOW,
				      &window_drawable_vtable,
				      this);

#ifdef QTFE_DOUBLE_BUFFER_IMPLEMENTED
  CL_Drawable *cl_backing_store_drawable;
  /* Create XP handle to backing store for compositor */
  cl_backing_store_drawable = CL_NewDrawable(comp_width, comp_height,
					     CL_BACKING_STORE,
					     &bs_drawable_vtable,
					     this);
#endif

  CL_Compositor* result = CL_NewCompositor(
	cl_window_drawable,
#ifdef QTFE_DOUBLE_BUFFER_IMPLEMENTED
	cl_backing_store_drawable,
#else
	0,
#endif
        0, 0, comp_width, comp_height, 5);

  CL_SetCompositorEnabled(result, (PRBool)true);

  return result;
}




/* From ./xfe.c: */
void QtContext::setRefreshURLTimer(uint32 secs, char *url)
{
    useArgs( "QtContext::setRefreshURLTimer %d, %s", secs, url);
}

/* From ./commands.c: */
void QtContext::connectToRemoteHost(int url_type, char *hostname,
			char *port, char *username)
{
    useArgs( "FE_ConnectToRemoteHost %d, %s, %s, %s \n", url_type,
	    hostname, port, username );
}

/* From ./xfe.c: */
int32 QtContext::getContextID()
{
  return((int32) this);
}

/* From ./lay.c: */
void QtContext::createEmbedWindow(NPEmbeddedApp *app)
{
    useArgs( "QTFE_CreateEmbedWindow %p \n", app );
}

/* From ./lay.c: */
void QtContext::saveEmbedWindow(NPEmbeddedApp *app)
{
    useArgs( "QTFE_SaveEmbedWindow %p \n", app );
}

/* From ./lay.c: */
void QtContext::restoreEmbedWindow(NPEmbeddedApp *app)
{
    useArgs( "QTFE_RestoreEmbedWindow %p \n", app );
}

/* From ./lay.c: */
void QtContext::destroyEmbedWindow(NPEmbeddedApp *app)
{
    useArgs( "QTFE_DestroyEmbedWindow %p \n", app );
}

/* From ./xfe.c: */
void QtContext::setDrawable(CL_Drawable * /*drawable*/)
{
    //This function is not necessary, according to Matthias
    //useArgs( "QTFE_SetDrawable %p \n", drawable );
}

/* From ./src/context_funcs.cpp: */
void QtContext::progress(const char * message)
{
    setupToolbarSignals();
    emit progressReport( message );
}

/* From ./mozilla.c: */
void QtContext::setCallNetlibAllTheTime()
{
    useArgs( "QTFE_SetCallNetlibAllTheTime  \n" );
}

/* From ./mozilla.c: */
void QtContext::clearCallNetlibAllTheTime()
{
    useArgs( "QTFE_ClearCallNetlibAllTheTime  \n" );
}



/* From ./src/context_funcs.cpp: */
void QtContext::graphProgressInit(URL_Struct *, int32 content_length)
{
    setupToolbarSignals();
    emit progressStarting( content_length );
}

/* From ./src/context_funcs.cpp: */
void QtContext::graphProgressDestroy(URL_Struct *,
				     int32, // content_length
				     int32 ) // total_bytes_read
{
    setupToolbarSignals();
    emit progressFinished();
    // ### "Document: done"?
}
/* From ./src/context_funcs.cpp: */
void QtContext::graphProgress(URL_Struct *,
				  int32,
				  int32, /* bytes_since_last_time */
				  int32 )
{
}

/* From ./xfe.c: */
int  QtContext::fileSortMethod()
{
    //printf( "QTFE_FileSortMethod  \n", window_id );
    return 0;
}

/* From ./lay.c: */
void QtContext::getEmbedSize(LO_EmbedStruct *embed_struct,
		  NET_ReloadMethod force_reload)
{
    useArgs( "QTFE_GetEmbedSize %p, %p \n", embed_struct,
	    (void*)force_reload );
}

/* From ./lay.c: */
void QtContext::getJavaAppSize(LO_JavaAppStruct *java_struct,
		    NET_ReloadMethod reloadMethod)
{
    useArgs( "QTFE_GetJavaAppSize %p, %p \n", java_struct,
	    (void*)reloadMethod );
}


/* From ./lay.c: */
void QtContext::freeEmbedElement(LO_EmbedStruct *embed_struct)
{
    useArgs( "QTFE_FreeEmbedElement %p", embed_struct);
}

/* From ./lay.c: */
void QtContext::freeJavaAppElement(struct LJAppletData *appletData)
{
    useArgs( "QTFE_FreeJavaAppElement %p", appletData);
}

/* From ./lay.c: */
void QtContext::hideJavaAppElement(struct LJAppletData *appletData)
{
    useArgs( "QTFE_HideJavaAppElement %p", appletData);
}

/* From ./lay.c: */
void QtContext::freeEdgeElement(LO_EdgeStruct *edge)
{
    useArgs( "QTFE_FreeEdgeElement %p", edge);
}

/* From ./src/context_funcs.cpp: */
void QtContext::setProgressBarPercent(int32 percent)
{
    setupToolbarSignals();
    emit progressMade( percent );
}

/* From ./lay.c: */
void QtContext::setBackgroundColor(uint8 red, uint8 green,
			uint8 blue)
{
    useArgs( "QTFE_SetBackgroundColor %d, %d, %d", red, green,
	    blue);
}

/* From ./lay.c: */
void QtContext::displayEmbed(int iLocation, LO_EmbedStruct *embed_struct)
{
    useArgs( "QTFE_DisplayEmbed %d, %p", iLocation, embed_struct);
}

/* From ./lay.c: */
void QtContext::displayJavaApp(int iLocation, LO_JavaAppStruct *java_struct)
{
    useArgs( "QTFE_DisplayJavaApp %d, %p", iLocation, java_struct);
}

/* From ./lay.c: */
void QtContext::displayEdge(int loc, LO_EdgeStruct *edge)
{
    useArgs( "QTFE_DisplayEdge %d, %p", loc, edge);
}


/* From ./lay.c: */
void QtContext::displaySubDoc(int loc, LO_SubDocStruct *sd)
{
    useArgs( "QTFE_DisplaySubDoc %d, %p", loc, sd);
}

/* From ./lay.c: */
void QtContext::displayLineFeed(int iLocation, LO_LinefeedStruct *line_feed, bool need_bg)
{
    // The only interesting thing to do is highlight if selected.
    useArgs( 0, iLocation, line_feed, (int)need_bg);
}


/* From ./lay.c: */
void QtContext::displayFeedback(int iLocation, LO_Element *element)
{
    static int counter;
    counter++;
    if ( counter%50 == 1 )
	useArgs( "QTFE_DisplayFeedback #%d:%d, %p", counter, iLocation, element);
}


/* From ./lay.c: */
void QtContext::clearView(int which)
{
  if (contentWidget()){
    contentWidget()->erase();
  }
}

/* From ./scroll.c: */
void QtContext::setDocDimension(int /*iLocation*/, int32 iWidth, int32 iLength)
{
    useArgs( "QTFE_SetDocDimension", iWidth, iLength);
}

/* From ./scroll.c: */
void QtContext::setDocPosition(int iLocation, int32 x, int32 y)
{
    useArgs( "QTFE_SetDocPosition %d, %d, %d", iLocation, x, y);
}

/* From ./scroll.c: */
void QtContext::getDocPosition(int /*iLocation*/,
		   int32 *iX, int32 *iY)
{
    *iX = documentXOffset();
    *iY = documentYOffset();
}

/* From ./src/context_funcs.cpp: */
void QtContext::enableClicking()
{
    //useArgs( "QTFE_EnableClicking" );
}

/* From ./lay.c: */
void QtContext::drawJavaApp(int /*iLocation*/, LO_JavaAppStruct *java_struct)
{
    useArgs( "QTFE_DrawJavaApp", java_struct);
}

/* From ./src/context_funcs.cpp: */
void QtContext::allConnectionsComplete()
{
    useArgs( "QTFE_AllConnectionsComplete" );
}

/* From ./fonts.c: */

/* From ./src/context_funcs.cpp: */
int QtContext::enableBackButton()
{
    return 0;
}

/* From ./src/context_funcs.cpp: */
int QtContext::enableForwardButton()
{
    return 0;
}

/* From ./src/context_funcs.cpp: */
int QtContext::disableBackButton()
{
    return 0;
}

/* From ./src/context_funcs.cpp: */
int QtContext::disableForwardButton()
{
    return 0;
}

/* Header file comment: */
/* This is called when there's a chance that the state of the
 * Stop button (and corresponding menu item) has changed.
 * The FE should use XP_IsContextStoppable to determine the
 * new state.
 */
/* From ./src/context_funcs.cpp: */
void QtContext::updateStopState()
{
    //#warning TODO this needs frame support, see src/context_funcs.cpp

    setupToolbarSignals();
    if ( XP_IsContextBusy(mwContext()) ) {
	// nothing?
    } else {
	emit progressReport( "Document done" );
    }

    emit canStop( XP_IsContextStoppable(mwContext()) );

#if defined DEBUG_ettrich
    useArgs( "FE_UpdateStopState" );
    printf("**********************************************\n");
    printf("XP_IsContextBusy = %d\n", XP_IsContextBusy(mwContext()));
    printf("XP_IsContextStoppable = %d\n", XP_IsContextStoppable(mwContext()));
    printf("Number of images: %d\n", mwContext()->images);
    printf("**********************************************\n");
#endif

}

/* From ./lay.c: */
void QtContext::getFullWindowSize(int32 *width, int32 *height)
{
  *width = visibleWidth();
  *height = visibleHeight();
}

/* From ./lay.c: */
void QtContext::getEdgeMinSize(int32 *size_p)
{
    useArgs( "FE_GetEdgeMinSize", size_p);
}


/* From ./images.c: */
void QtContext::shiftImage(LO_ImageStruct *lo_image)
{
    useArgs( "FE_ShiftImage", lo_image);
}

/* From ./scroll.c: */
void QtContext::scrollDocTo(int /*iLocation*/, int32 x, int32 y)
{
    useArgs( "FE_ScrollDocTo", x, y);
}

/* From ./scroll.c: */
void QtContext::scrollDocBy(int /*iLocation*/, int32 deltax, int32 deltay)
{
    useArgs( "FE_ScrollDocBy", deltax, deltay);
}

/* From ./src/context_funcs.cpp: */
void QtContext::backCommand()
{
    useArgs( "FE_BackCommand" );
}

/* From ./src/context_funcs.cpp: */
void QtContext::forwardCommand()
{
    useArgs( "FE_ForwardCommand" );
}

/* From ./src/context_funcs.cpp: */
void QtContext::homeCommand()
{
    useArgs( "FE_HomeCommand" );
}

/* From ./src/context_funcs.cpp: */
void QtContext::printCommand()
{
    useArgs( "FE_PrintCommand" );
}


void QtContext::getWindowOffset(int32 *sx, int32 *sy)
{
    if ( contentWidget() ) {
	QPoint gp = contentWidget()->mapToGlobal( QPoint(0,0) );
	*sx = gp.x();
	*sy = gp.y();
    }
}


void QtContext::getScreenSize(int32 *sx, int32 *sy)
{
    *sx = qApp->desktop()->width();
    *sy = qApp->desktop()->height();
}


void QtContext::getAvailScreenRect(int32 *sx, int32 *sy,
				   int32 *left, int32 *top)
{
    *left = *top = 0;
    getScreenSize( sx, sy );
}


/* From ./xfe.c: */
void QtContext::setWindowLoading(URL_Struct *url,
	Net_GetUrlExitFunc **exit_func_p)
{
    useArgs( "FE_SetWindowLoading", url,
	exit_func_p);
}

/* From ./forms.c: */
void QtContext::raiseWindow()
{
    useArgs( "FE_RaiseWindow" );
}

/* From ./src/context_funcs.cpp: */
void QtContext::destroyWindow()
{
    useArgs( "FE_DestroyWindow" );
}

/* From ./editor.c: */
void QtContext::getDocAndWindowPosition(int32 *pX, int32 *pY,
    int32 *pWidth, int32 *pHeight )
{
    useArgs( "void FE_GetDocAndWindowPosition", pX, pY,
    pWidth, pHeight );
}

/* From ./editor.c: */
void QtContext::displayTextCaret(int /*iLocation*/, LO_TextStruct* text,
		    int char_offset)
{
    useArgs( "FE_DisplayTextCaret", text,
		    char_offset);
}

/* From ./editor.c: */
void  QtContext::displayImageCaret(LO_ImageStruct*        image,
		     ED_CaretObjectPosition caretPos)
{
    useArgs( "FE_DisplayImageCaret",
		     image,
		     caretPos);
}

/* From ./editor.c: */
void QtContext::displayGenericCaret(LO_Any * image,
                        ED_CaretObjectPosition caretPos )
{
    useArgs( "void FE_DisplayGenericCaret", image,
                        caretPos );
}

/* From ./editor.c: */
bool  QtContext::getCaretPosition(LO_Position* where,
		    int32* caretX, int32* caretYLow, int32* caretYHigh
)
{
    useArgs( "FE_GetCaretPosition",
		    where,
		    caretX, caretYLow, caretYHigh );
    return 0;
}

/* From ./editor.c: */
void QtContext::destroyCaret()
{
    useArgs( "FE_DestroyCaret" );
}

/* From ./editor.c: */
void QtContext::showCaret()
{
    useArgs( "FE_ShowCaret" );
}

/* From ./editor.c: */
void  QtContext::documentChanged(int32 p_y, int32 p_height)
{
    useArgs( "FE_DocumentChanged", p_y, p_height);
}

/* From ./editor.c: */
void QtContext::setNewDocumentProperties()
{
    useArgs( "FE_SetNewDocumentProperties" );
}

/* Header file comment: */
/*
 * Brings up a modal image load dialog and returns.  Calls
 *  EDT_ImageLoadCancel() if the cancel button is pressed
*/
/* From ./editor.c: */
void QtContext::imageLoadDialog()
{
    useArgs( "FE_ImageLoadDialog" );
}

/* From ./editor.c: */
void QtContext::imageLoadDialogDestroy()
{
    useArgs( "FE_ImageLoadDialogDestroy" );
}

/* Header file comment: */
/*
 * Bring up a files saving progress dialog.  While the dialog is modal to the edit
 *  window. but should return immediately from the create call.  If cancel button
 *  is pressed, EDT_SaveCancel(pMWContext) should be called if saving locally,
 *  or  NET_InterruptWindow(pMWContext) if uploading files
 *  Use bUpload = TRUE for Publishing file and images to remote site
*/
/* From ./editor.c: */
void  QtContext::saveDialogCreate(int nfiles, ED_SaveDialogType saveType)
{
    useArgs( "FE_SaveDialogCreate", nfiles, saveType);
}

/* From ./editor.c: */
void  QtContext::saveDialogSetFilename(char* filename)
{
    useArgs( "FE_SaveDialogSetFilename", filename);
}

/* Header file comment: */
/* Sent after file is completely saved, even if user said no to overwrite
 *  (then status = ED_ERROR_FILE_EXISTS)
 * Not called if failed to open source file at all, but is called with error
 *   status if failed during writing
*/
/* From ./editor.c: */
void  QtContext::finishedSave(int status,
		char *pDestURL, int iFileNumber)
{
    useArgs( "FE_FinishedSave", status,
		pDestURL, iFileNumber);
}

/* From ./editor.c: */
void  QtContext::saveDialogDestroy(int status, char* file_url)
{
    useArgs( "FE_SaveDialogDestroy", status, file_url);
}

/* Header file comment: */
/* Sent after file is opened (or failed) -- same time saving is initiated */
/* From ./editor.c: */
bool QtContext::saveErrorContinueDialog(char*        filename,
			   ED_FileError error)
{
    useArgs( "FE_SaveErrorContinueDialog", filename,
			   error);
    return 0;
}

/* From ./images.c: */
void QtContext::clearBackgroundImage()
{
    useArgs( "FE_ClearBackgroundImage" );
}

/* From ./editor.c: */
void QtContext::editorDocumentLoaded()
{
    useArgs( "FE_EditorDocumentLoaded" );
}

/* Header file comment: */
/* Called during "sizing" of a table when we are really adding/subtracting
 * rows or columns. Front end should draw a line
 *   from the point {rect.left, rect.top} to the point {rect.right,rect.bottom}
 *   (Thickness and style are left to FE, Windows uses a dashed line)
 *   This will be called with bErase = TRUE to remove the line when appropriate,
 *   so FEs don't need to worry about removing selection,
 *   but you must call EDT_CancelSizing or EDT_EndSizing for final removal.
*/
/* From ./editor.c: */
void QtContext::displayAddRowOrColBorder(XP_Rect *pRect,
                            bool bErase)
{
    useArgs( "FE_DisplayAddRowOrColBorder", pRect,
                            bErase);
}

/* From ./editor.c: */
void  QtContext::displayEntireTableOrCell(LO_Element* pLoElement)
{
    useArgs( "FE_DisplayEntireTableOrCell", pLoElement);
}

/* Header file comment: */
/* This is how the libraries ask the front end to load a URL just
   as if the user had typed/clicked it (thermo and busy-cursor
   and everything.)  NET_GetURL doesn't do all this stuff.
 */
/* From ./xfe.c: */
int QtContext::getURL(URL_Struct *url)
{
    //#### we obviously need to check whether we are reloading the
    //#### same document

    useArgs( "FE_GetURL", url);
    return 0;
}

/* From ./forms.c: */
void QtContext::focusInputElement(LO_Element *element)
{
    useArgs( "FE_FocusInputElement", element);
}

/* From ./forms.c: */
void QtContext::blurInputElement(LO_Element *element)
{
    useArgs( "FE_BlurInputElement", element);
}

/* From ./forms.c: */
void QtContext::selectInputElement(LO_Element *element)
{
    useArgs( "FE_SelectInputElement", element);
}

/* From ./forms.c: */
void QtContext::changeInputElement(LO_Element *element)
{
    useArgs( "FE_ChangeInputElement", element);
}

/* Header file comment: */
/*
 * Tell the FE that a form is being submitted without a UI gesture indicating
 * that fact, i.e., in a Mocha-automated fashion ("document.form.submit()").
 * The FE is responsible for emulating whatever happens when the user hits the
 * submit button, or auto-submits by typing Enter in a single-field form.
 */
/* From ./forms.c: */
void QtContext::submitInputElement(LO_Element *element)
{
    // In fact, we should share this code with the real submit.
    // See submit_form in forms.cpp
    useArgs( "FE_SubmitInputElement", element);
}

/* From ./forms.c: */
void QtContext::clickInputElement(LO_Element *xref)
{
    useArgs( "FE_ClickInputElement", xref);
}

/* From ./xfe.c: */
bool QtContext::handleLayerEvent(CL_Layer *layer,
                           CL_Event *layer_event)
{
    useArgs( "bool FE_HandleLayerEvent", layer,
                           layer_event);
    return (bool)0;
}

/* From ./xfe.c: */
bool QtContext::handleEmbedEvent(LO_EmbedStruct *embed,
                           CL_Event *event)
{
    useArgs( "bool FE_HandleEmbedEvent", embed,
                           event);
    return (bool)0;
}

/* From ./src/context_funcs.cpp: */
void QtContext::updateChrome(Chrome *chrome)
{
    useArgs( "FE_UpdateChrome", chrome);
}

/* From ./src/context_funcs.cpp: */
void QtContext::queryChrome(Chrome * chrome)
{
    useArgs( "FE_QueryChrome", chrome);
}

/* -------------------------------------------------------------------------
 * Grid stuff (where should this go?)
 */
MWContext * QtContext::makeGridWindow(void *hist_list, void *history,
	int x, int y, int width, int height,
	char *url_str, char *window_name, int scrolling,
	NET_ReloadMethod force_reload, bool no_edge
    )
{
  URL_Struct * url = url_str?NET_CreateURLStruct (url_str, NET_NORMAL_RELOAD):0;

  MWContext* context = XP_NewContext();
  context->type = MWContextBrowser;
  context->funcs = &qtbind;
  context->is_grid_cell = TRUE;
  context->grid_parent = mwContext();
  XP_AddContextToList (context);

  int  bw = no_edge?0:4;
  QtContext* qc = new QtBrowserContext(context, 0, contentWidget(), window_name, scrolling, bw);
  qc->topLevelWidget()->setGeometry(x, y, width, height);
  context->compositor = qc->createCompositor(context);
  qc->getURL( url);
  return context;
}

void QtContext::restructureGridWindow(int32 x, int32 y,
				      int32 width, int32 height){
  topLevelWidget()->setGeometry(x,y,width,height);
}

/* From ./lay.c: */
void QtContext::loadGridCellFromHistory(void *hist,
			   NET_ReloadMethod force_reload)
{
  History_entry *he = (History_entry *)hist;
  URL_Struct *url;
  if (! he) return;
  url = SHIST_CreateURLStructFromHistoryEntry (mwContext(), he);
  url->force_reload = force_reload;
  getURL(url);

}

/* From ./lay.c: */
void * QtContext::freeGridWindow(bool save_history)
{
  XP_List *hist_list = 0;

// TODO handling of save_history fort his cell (see lay.c)
//   if (save_history)


  if (XP_IsContextBusy(mwContext())){
    destroyed = true;
  }
  else {
    XP_RemoveContextFromList(mwContext());
    delete this;
  }

  return hist_list;
}


/* From ./xfe-dns.c: */
void QtContext::clearDNSSelect(int socket)
{
  useArgs( "Sorry, not implemented: FE_ClearDNSSelect %d\n", socket );
}

/* From ./xfe.c: */
void QtContext::setReadSelect(int fd)
{
  useArgs(  "Sorry, not implemented: FE_SetReadSelect %d\n", fd );
}

/* From ./xfe.c: */
void QtContext::clearReadSelect(int fd)
{
  useArgs( "Sorry, not implemented: FE_ClearReadSelect %d \n", fd );
}

/* Header file comment: */
/* tell the front end to call ProcessNet with fd when fd
 * has connected. (write select and exception select)
 */
/* From ./xfe.c: */
void QtContext::setConnectSelect(int fd)
{
  useArgs( "Sorry, not implemented: FE_SetConnectSelect %d\n", fd );
}

/* From ./xfe.c: */
void QtContext::clearConnectSelect(int fd)
{
  useArgs( "Sorry, not implemented: FE_ClearConnectSelect %d\n", fd );
}

/* From ./xfe.c: */
void QtContext::setFileReadSelect(int fd)
{
  useArgs( "Sorry, not implemented: FE_SetFileReadSelect %d\n", fd );
}

/* From ./xfe.c: */
void QtContext::clearFileReadSelect(int fd)
{
  useArgs( "Sorry, not implemented: FE_ClearFileReadSelect %d\n", fd );
}

/* #### this should be replaced with a backup-version-hacking XP_FileOpen */
char*  QtContext::getTempFileFor(const char* fname,
			   XP_FileType ftype, XP_FileType* rettype)
{
    printf( "E_GetTempFileFor %s, %p, %p \n", fname,
			   (void*)ftype, rettype );
    return 0;
}


void QtContext::updateRect(int x, int y, int w, int h)
{
    if (context->compositor && contentWidget()) {

    //fprintf(stderr, "Update RECT for %p: %d %d %d %d \n", this, x, y, w, h);

	CL_CompositeNow(context->compositor);

	XP_Rect rect;

	rect.left = x;
	rect.top = y;
	rect.right = x+w;  // no -1 because it's an XRectangle structure
	rect.bottom = y+h;

	CL_UpdateDocumentRect((context)->compositor, &rect, PR_TRUE);
    }
}

void QtContext::getPixelAndColorDepth(int32 *pixelDepth,
				      int32 *colorDepth)
{
    // To simplify, we pretend we always have a true color system,
    // then Qt can deal with the color allocation later.
    *pixelDepth = *colorDepth = 32;
}


void QtContext::setTransparentPixel( const QColor &color )
{
    MWContext *c = mwContext();
    IL_IRGB *trans_pixel = c->transparent_pixel;

    if (!trans_pixel) {
        trans_pixel = c->transparent_pixel = XP_NEW_ZAP(IL_IRGB);
        if (!trans_pixel)
            return;
    }

    /* Set the color of the transparent pixel. */
    trans_pixel->red = color.red();
    trans_pixel->green = color.green();
    trans_pixel->blue = color.blue();
}

QPainter* QtContext::makePainter()
{
    return 0;
}

QPainter* QtContext::forcePainter() const
{
    QtContext* that = (QtContext*)this;
    that->internal_painter = that->makePainter();
    return that->internal_painter;
}


void QtContext::perror( const char* message )
{
    fprintf( stderr, "*********** %s\n", message );
    useArgs( "Sorry, not implemented: QtContext::perror" );
}


bool QtContext::isGridParent( MWContext* context ) const
{
  return( context->grid_children &&
	  XP_ListTopObject( context->grid_children ) );
}


void QtContext::cmdQuit()
{
    // #warning Probably something more fancy is needed here.
    qApp->quit();
}


int QtContext::getSecondaryURL( URL_Struct *url_struct,
				int output_format,
				void *call_data, bool skip_get_url )
{
#if 0//defined DEBUG_paul
    debug( "getSecondaryURL %d", skip_get_url );
#endif


    /* The URL will be freed when libnet calls the fe_url_exit() callback. */

    /* If the URL_Struct already has fe_data, try to preserve it.  (HTTP publishing in the
       editor, EDT_PublishFileTo(), uses it.) */
    if ( call_data ) {
	XP_ASSERT( !url_struct->fe_data );
	url_struct->fe_data = call_data;
    }
    /* else leave url_struct->fe_data alone */


    active_url_count++;
    if( active_url_count == 1 ) {
	clicking_blocked = true;
	if( mwContext()->is_grid_cell ) {
	    MWContext *top_context = mwContext();
	    while ((top_context->grid_parent)&&
		   (top_context->grid_parent->is_grid_cell))
		{
	      top_context = top_context->grid_parent;
            }
          fe_StartProgressGraph (top_context->grid_parent);
        }
      else
        {
	  fe_StartProgressGraph (context);
        }

	fe_SetCursor (context, false);
	
	/* Enable stop here (uses active_url_count)
	 * This makes the stop button active during
	 * host lookup and initial connection.
	 */
	FE_UpdateStopState(context);
    }

    if (skip_get_url)
	{
	    return(0);
	}

    return NET_GetURL (url_struct, output_format, context, fe_url_exit);
}


void
QtContext::saveURL ( URL_Struct *url )
{
  struct save_as_data *sad;
  bool text_p = true;

  assert (url);
  if (!url) return;

  sad = (struct save_as_data*)make_save_as_data (context, text_p,
						 SaveAsDialog::Source,
						 url, 0 );
  if (sad)
    fe_save_as_nastiness (this, url, sad, FALSE);
  else
    NET_FreeURLStruct (url);
}
