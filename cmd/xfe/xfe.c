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
   xfe.c --- other stuff specific to the X front end.
   Created: Jamie Zawinski <jwz@netscape.com>, 22-Jun-94.
 */


#include "mozilla.h"
#include "altmail.h"
#include "xfe.h"
#include "fonts.h"
#include "felocale.h"
#include "intl_csi.h"
#include "selection.h"
#include "rdf.h"
#ifdef NSPR20
#include "private/prpriv.h"	/* for PR_GetMonitorEntryCount */
#endif /* NSPR20 */

#ifndef NSPR20
#include <prevent.h>		/* for mocha */
#else
#include <plevent.h>		/* for mocha */
#endif
#include <prtypes.h>
#include <libevent.h>
#include <xp_list.h>

/* Layering support - LO_RefreshArea is called through compositor */
#include "layers.h"

#include <X11/IntrinsicP.h>
#include <X11/ShellP.h>
#include <X11/Xatom.h>
#ifdef DEBUG
#include <X11/Xmu/Editres.h>	/* for editres to work */
#endif

#include <Xfe/Xfe.h>			/* for xfe widgets and utilities */

#ifdef EDITOR
#include "xeditor.h"
#endif /* EDITOR */
#include <Xm/Label.h>

#ifdef AIXV3
#include <sys/select.h>
#endif /* AIXV3 */

#include <sys/time.h>

#include <np.h>

#include "msgcom.h"
#include "secnav.h"
#include "mozjava.h"

#include "libmocha.h"
#include "libevent.h"

#include "prefapi.h"
#include "NSReg.h"
#ifdef MOZ_SMARTUPDATE
#include "softupdt.h"
#endif

#if defined(_HPUX_SOURCE)
/* I don't know where this is coming from...  "ld -y Error" says
   /lib/libPW.a(xlink.o): Error is DATA UNSAT
   /lib/libPW.a(xmsg.o): Error is DATA UNSAT
 */
int Error = 0;
#endif

#include "libi18n.h"

#include "libimg.h"             /* Image Library public API. */
#include "il_util.h"            /* Colormap/colorspace utilities. */

/* for XP_GetString() */
#include <xpgetstr.h>

#ifndef NO_WEB_FONTS
#include "nf.h"
#include "Mnfrc.h"
#include "Mnfrf.h"
#endif

#include "XmL/GridP.h"
#include "XmL/Grid.h"

extern int XFE_DOCUMENT_DONE;
extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_STDERR_DIAGNOSTICS_HAVE_BEEN_TRUNCATED;
extern int XFE_ERROR_CREATING_PIPE;
extern int XFE_DISPLAY_FACTORY_INSTALL_COLORMAP_ERROR;

extern char *fe_calendar_path;
extern char *fe_host_on_demand_path;
extern char *fe_conference_path;

extern MWContext *last_documented_xref_context;
extern LO_Element *last_documented_xref;
extern LO_AnchorData *last_documented_anchor_data;

int fe_WindowCount = 0;

/*
 * GLOBALS for new fe_find_scrollbar_sizes mechanism...
 */
int fe_ScrollBarW = 0;
int fe_ScrollBarH = 0;
Boolean fe_SBW_P = False;
Boolean fe_SBH_P = False;

MWContext *someGlobalContext = NULL;
static Boolean has_session_mgr= False;

#ifdef MOZ_MAIL_NEWS
MSG_Master* fe_mailMaster = NULL; 
MSG_Master* fe_newsMaster = NULL;
#endif

struct fe_MWContext_cons *fe_all_MWContexts = 0;

void fe_delete_cb (Widget, XtPointer, XtPointer);

static void fe_save_history_timer (XtPointer closure, XtIntervalId *id);
extern void fe_MakeAddressBookWidgets(Widget shell, MWContext *context);

extern void fe_ab_destroy_cb (Widget, XtPointer, XtPointer);
#ifdef UNIX_LDAP
extern void fe_ldapsearch_destroy_cb(Widget, XtPointer, XtPointer);
#endif
extern XP_Bool fe_ReadLastUserHistory(char **hist_entry_ptr);

#define DO_NOT_PASS_LAYER_N_EVENT 1
#define HANDLE_LEAVE_WIN          1

#if DO_NOT_PASS_LAYER_N_EVENT
extern void
fe_HTMLViewTooltips(MWContext *context, 
					int x, int y, char *alt_text, 
					XFE_TipStringCallbackStruct *cb_info);
#endif

/*
 * Keep track of tooltip mapping to avoid conflict with fascist shells
 * that insist on raising themselves - like taskbar and netcaster webtop
 */
static Boolean fe_tooltip_is_showing = False;

#if XmVersion >= 2000
#define _XM_OBJECT_AT_POINT XmObjectAtPoint
#else
#define _XM_OBJECT_AT_POINT _XmInputInGadget
#endif

void
fe_url_exit (URL_Struct *url, int status, MWContext *context)
{
  XP_ASSERT (status == MK_INTERRUPTED ||
			 CONTEXT_DATA (context)->active_url_count > 0);

  if (CONTEXT_DATA (context)->active_url_count > 0)
    CONTEXT_DATA (context)->active_url_count--;

  /* We should be guarenteed that XFE_AllConnectionsComplete() will be called
     right after this, if active_url_count has just gone to 0. */
  if (status < 0 && url->error_msg)
    {
      FE_Alert (context, url->error_msg);
    }
#if defined(EDITOR) && defined(EDITOR_UI)
  /*
   *    Do stuff specific to the editor
   */
  if (context->type == MWContextEditor)
    fe_EditorGetUrlExitRoutine(context, url, status);
#endif
  if (status != MK_CHANGING_CONTEXT)
    {
      NET_FreeURLStruct (url);
    }
}

/* this func is basically XP_FindContextOfType but only returns
 * a top-level non-nethelp browser context with chrome
 */
MWContext*
fe_FindNonCustomBrowserContext(MWContext *context)
{
	XP_List *context_list;
	int      n_context_list;
	int i;

	/* If our current context has the right type, go there */
	if (context && context->type == MWContextBrowser &&
		!context->is_grid_cell
		&& !CONTEXT_DATA(context)->hasCustomChrome
		&& context->pHelpInfo == NULL
		&& !(context->name != NULL &&  /* don't use view source either */
			 XP_STRNCMP(context->name, "view-source", 11) == 0)) {
	  return context;
	}

	/* otherwise, just get any other Browser context */
	context_list = XP_GetGlobalContextList();
	n_context_list = XP_ListCount(context_list);
	for (i = 1; i <= n_context_list; i++) {
		MWContext * compContext = 
			(MWContext *)XP_ListGetObjectNum(context_list, i);
		if (compContext->type == MWContextBrowser &&
			!compContext->is_grid_cell
			&& !CONTEXT_DATA(compContext)->hasCustomChrome
			&& compContext->pHelpInfo == NULL
			&& !(context->name != NULL &&  /* don't use view source either */
				 XP_STRNCMP(context->name, "view-source", 11) == 0)) {
			/*
			 * NOTE:  make sure that this window is "open"...
			 *
			 */
			FE_RaiseWindow(compContext);
			return compContext;
		}
	}
	return NULL;
}

int
FE_GetURL (MWContext *context, URL_Struct *url)
{
	if (!context)
		return -1;

	/*
	 *    Rules lifted from winfe:
	 *
	 *    The only circumstance in which we load a URL
	 *    into Editor when called from XP code is during
	 *    Publishing: we have "files to post". In general, the
	 *    editor does not use fe_GetURL(). Editor BE has special
	 *    calls that deal with the special pushups an editor context
	 *    needs. This code delegates geturl() requests made of an editor
	 *    to a browser context. This will happen for help, nethelp,
	 *    about:, etc...djw
	 */
    if (EDT_IS_EDITOR(context) && url->files_to_post == NULL) {
		
		MWContext* real_context;
		
		real_context = fe_FindNonCustomBrowserContext(context);
		
		if (!real_context) {
			real_context = FE_MakeNewWindow(context, NULL, NULL, NULL);
		}

		context = real_context;
    }
	
	return fe_GetURL (context, url, FALSE);
}


extern int XFE_DesktopTypeTranslate(const char*,char**,int);

extern Boolean 
xfe_NewWindowRequired(MWContext *context, const char *url)
{
#ifdef MOZ_MAIL_NEWS
  MSG_Pane *msgPane = MSG_FindPaneOfContext(context, MSG_MESSAGEPANE);
  MSG_Pane *threadPane = MSG_FindPaneOfContext(context, MSG_THREADPANE);
  MSG_Pane *folderPane = MSG_FindPaneOfContext(context, MSG_FOLDERPANE);
#endif
  if (!context)
          return True;

#ifdef MOZ_MAIL_NEWS
  if (folderPane && MSG_RequiresNewsWindow(url))
     return False;
#endif

  return True;
}

/* View the URL in the given context.
   A pointer to url_string is not retained.
   Returns the status of NET_GetURL().
   */
int
fe_GetURL (MWContext *context, URL_Struct *url, Boolean skip_get_url)
{
  int32 x, y;
  LO_Element *e;
  History_entry *he;
  MWContext *new_context = NULL;
  char *new_address=NULL;
  URL_Struct *desktop_url=NULL;
  Boolean needNewWindow = False;  
  /*
   * See if the URL points to a local file which is a Netscape desktop
   * file type. If so, extract the url from the file and create a new
   * URL_Struct. (added for desktop dnd integration - Alastair.)
   */

  if (XFE_DesktopTypeTranslate(url->address,&new_address,TRUE)) {
      if (new_address && XP_STRLEN(new_address)>0) {
          desktop_url=NET_CreateURLStruct (new_address,NET_DONT_RELOAD);
          if (desktop_url) {
              NET_FreeURLStruct(url);
              url=desktop_url;
          }
      }
  }
  
#ifdef JAVA
  /*
  ** This hack is to get around the crash when we have an
  ** auto-config proxy installed, and we come up in the mail
  ** window and get a message with an applet on it:
  */
  if (context->type == MWContextJava) {
      context = NSN_JavaContextToRealContext(context);
  }
#endif

#ifdef MOZ_MAIL_NEWS
  /* If this URL requires a particular kind of window, and this is not
     that kind of window, then we need to find or create one.
   */
  if (!skip_get_url && 
#ifdef EDITOR
      !(context->is_editor) &&
#endif
      (needNewWindow = MSG_NewWindowRequired(context, url->address)) &&
      XP_STRCMP(url->address, "search-libmsg:"))
    {

      XP_ASSERT (!MSG_NewWindowProhibited (context, url->address));
     
      if ( context->type != MWContextSearch
		   && context->type != MWContextSearchLdap
		   /* Skip for Nethelp which  will create its
			* own new window if it is needed */
		   && NET_URL_Type(url->address) != NETHELP_TYPE_URL )

	{

	  if ( MSG_RequiresBrowserWindow(url->address) )
	    {
		new_context = fe_FindNonCustomBrowserContext(context);
	    }

	  if ( new_context )
	    {
			/* If we find a new browser context, use it to display URL 
			 */
			if (context != new_context) {
				/* If we have picked an existing context that isn't this
				 * one in which to display this document, make sure that
				 * context is uniconified and raised first. 
				 */

				/* Popup the shell first, so that we gurantee its realized 
				 */
				XtPopup(CONTEXT_WIDGET(new_context),XtGrabNone);

				/* Force the window to the front and de-iconify if needed 
				 */
				XMapRaised(XtDisplay(CONTEXT_WIDGET(new_context)),
						   XtWindow(CONTEXT_WIDGET(new_context)));
			}
			context = new_context;
	    }
	}
    }
#endif /* MOZ_MAIL_NEWS */

  e = LO_XYToNearestElement (context,
			     CONTEXT_DATA (context)->document_x,
			     CONTEXT_DATA (context)->document_y,
                             NULL);

  he = SHIST_GetCurrent (&context->hist);

  if (e)
    SHIST_SetPositionOfCurrentDoc (&context->hist, e->lo_any.ele_id);

  /* LOU: don't do this any more since Layout will do it
   * for you once the page starts loading
   *
   * Here is why we have to do this for now...
   * 
   * | This is important feedback on this new "feature" (or misfeature as
   * | it may turn out).
   * 
   * If this is the feature where the front end no longer calls
   * XP_InterruptContext() inside fe_GetURL(), but only when data from
   * the new URL arrives, then I think I understand what's going on.
   * 
   * | I didn't expect the images that are currently transfering to interfere
   * | significantly with the TCP connection setup,
   * 
   * They won't interfere within TCP or IP modules on intervening routers.
   * ISDN signalling guarantees perfect scheduling of the D-channel; modem
   * link layers are fair.
   * 
   * The problem is this: before, when fe_GetURL() called XP_InterruptContext(),
   * the connections for the old document were closed immediately.  Those TCP
   * closes turned into packets with the FIN flag set on the wire, which went to
   * the server over the fairly-scheduled link.  The client moved into TCP state
   * FIN_WAIT_1, indicating that it had sent a FIN but needed an ACK, and that
   * the server had not yet closed its half of the connection.  As soon as the
   * next TCP segment for this connection arrived at the client, the client TCP
   * noticed that the user process had closed, so it aborted the connection by
   * sending a RST.
   * 
   * Since the change to the front ends, it appears the old connections don't
   * (all) get closed until data on the new connection arrives.  The data for
   * those old connections that are not yet closed will still be queued in the
   * server's TCP, and sent as the client ACKs previous data and updates the
   * server's window.  There is no way for an HTTP daemon to cancel the write
   * or send syscalls that enqueued this data, once the syscall has returned.
   * Only a RST from the client for each old connection will cause the server
   * to kill its enqueued data.
   * 
   * I doubt there is much data in the fairly narrow pipe between server and
   * client.  Because the pipe is narrow, the server TCP has not opened its
   * congestion window, and there cannot be much data in flight.  The problem
   * is lack of abort because of lack of close() when the new URL is clicked.
   * 
   * | Perhaps there is some TCP magic that we can do
   * | to cause the new stream to have absolute priority over other TCP
   * | streams.
   * 
   * No such magic, nor any need for it in this scenario.
   * 
   * | I could also pause all the transfering connections while
   * | we are waiting.
   * 
   * Only a close, which guarantees an abort when more old data arrives, will
   * do the trick.  Anything else requires server mods as well as client.
   */
  if (!skip_get_url &&
      XP_STRNCMP("view-source:", url->address, 12) != 0) {
    XP_InterruptContext (context); 
  }

  if (CONTEXT_DATA (context)->refresh_url_timer)
    {
      XtRemoveTimeOut (CONTEXT_DATA (context)->refresh_url_timer);
      CONTEXT_DATA (context)->refresh_url_timer = 0;
    }
  if (CONTEXT_DATA (context)->refresh_url_timer_url)
    {
      free (CONTEXT_DATA (context)->refresh_url_timer_url);
      CONTEXT_DATA (context)->refresh_url_timer_url = 0;
    }

  if (url && XP_FindNamedAnchor (context, url, &x, &y))
    {
      char *temp;
      URL_Struct *urlcp;
      
      fe_ScrollTo (context, 0, y);

	  if (! CONTEXT_DATA (context)->is_resizing) {
		fe_SetURLString (context, url);
	  }

      /* Create URL from prev history entry to preserve security, etc. */
      urlcp = SHIST_CreateURLStructFromHistoryEntry(context, he);

      /*  Swap addresses. */
      temp = url->address;
      url->address = urlcp->address;
      urlcp->address = temp;
	
      /* set history_num correctly */
      urlcp->history_num = url->history_num;
      
      /*  Free old URL, and reassign. */
      NET_FreeURLStruct(url);
      url = urlcp;
      NET_FreeURLStruct (url);
      fe_RefreshAllAnchors ();
      return 0;
    }
  else
    {
#ifdef EDITOR
      /*
       *    We have to pass "about:" docs through the old way,
       *    cause there's all this magic that happens! We do trap
       *    these in fe_EditorNew() so the user will not be allowed
       *    to edit them, but *we* do GetURLs on them as well.
       */
      if (EDT_IS_EDITOR(context) && strncasecmp(url->address, "about:", 6)!= 0)
	  return fe_GetSecondaryURL (context, url, FO_EDIT, 0,
				        skip_get_url);
      else
#endif
	  return fe_GetSecondaryURL (context, url, FO_CACHE_AND_PRESENT, 0,
					skip_get_url);
    }
}


/* Start loading a URL other than the main one (ie, an embedded image.)
   A pointer to url_string is not retained.
   Returns the status of NET_GetURL().
 */
int
fe_GetSecondaryURL (MWContext *context,
		    URL_Struct *url_struct,
		    int output_format,
		    void *call_data, Boolean skip_get_url)
{
  /* The URL will be freed when libnet calls the fe_url_exit() callback. */

  /* If the URL_Struct already has fe_data, try to preserve it.  (HTTP publishing in the 
     editor, EDT_PublishFileTo(), uses it.) */
  if (call_data) {
    XP_ASSERT(!url_struct->fe_data);
    url_struct->fe_data = call_data;
  }
  /* else leave url_struct->fe_data alone */


  CONTEXT_DATA (context)->active_url_count++;
  if (CONTEXT_DATA (context)->active_url_count == 1)
    {
      CONTEXT_DATA (context)->clicking_blocked = True;

      if (context->is_grid_cell)
        {
          MWContext *top_context = context;
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

      fe_SetCursor (context, False);

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

static XtInputId fe_fds_to_XtInputIds [FD_SETSIZE] = { 0, };

static void
fe_stream_callback (void *closure, int *source, XtInputId *id)
{
#ifdef QUANTIFY
quantify_start_recording_data();
#endif /* QUANTIFY */

#ifdef NSPR20_DISABLED
  NET_ProcessNet (*source, NET_UNKNOWN_FD);
#endif

#ifdef QUANTIFY
quantify_stop_recording_data();
#endif /* QUANTIFY */
}

static void
fe_add_input (int fd, int mask)
{
  if (fd < 0 || fd >= countof (fe_fds_to_XtInputIds))
    abort ();

  LOCK_FDSET();
  
#ifdef UNIX_ASYNC_DNS
  if (fe_UseAsyncDNS() && fe_fds_to_XtInputIds [fd]) {
	  /* If we're already selecting input on this fd, don't select it again
		 and lose our pointer to the XtInput object..  This shouldn't happen,
		 but netlib does this when async DNS lookups are happening.  (This
		 will lose if the `mask' arg has changed on two consecutive calls
		 without an intervening call to `fe_remove_input', but that doesn't
		 happen.)  -- jwz, 9-Jan-97.
		 */
	  goto DONE;
  }
#endif

  fe_fds_to_XtInputIds [fd] = XtAppAddInput (fe_XtAppContext, fd,
					     (XtPointer) mask,
					     fe_stream_callback, 0);
#ifdef JAVA
  if (PR_CurrentThread() != mozilla_thread) {
      /*
      ** Sometimes a non-mozilla thread will be using the netlib to fetch
      ** data. Because mozilla is the only thread that runs the netlib
      ** "select" code, we need to be able to kick mozilla and wake it up
      ** when the select set has changed.
      **
      ** A way to do this would be to have mozilla stop calling select
      ** and instead manually manipulate the idle' thread's select set,
      ** but there is yet to be an NSPR interface at that level.
      */
      PR_PostEvent(mozilla_event_queue, NULL);
  }
#endif /* JAVA */

#ifdef UNIX_ASYNC_DNS
DONE:
#endif
  UNLOCK_FDSET();
}

static void
fe_remove_input (int fd)
{
  if (fd < 0 || fd >= countof (fe_fds_to_XtInputIds))
    return;	/* was abort() --  */

  LOCK_FDSET();

  if (fe_fds_to_XtInputIds [fd] != 0) {
      XtRemoveInput (fe_fds_to_XtInputIds [fd]);
      fe_fds_to_XtInputIds [fd] = 0;
  }

  UNLOCK_FDSET();
}

void
FE_SetReadSelect (MWContext *context, int fd)
{
  fe_add_input (fd, XtInputReadMask | XtInputExceptMask);
}

void
FE_SetConnectSelect (MWContext *context, int fd)
{
  fe_add_input (fd, (XtInputWriteMask | XtInputExceptMask));
}

void
FE_ClearReadSelect (MWContext *context, int fd)
{
  fe_remove_input (fd);
}

void
FE_ClearConnectSelect (MWContext *context, int fd)
{
  FE_ClearReadSelect (context, fd);
}

void
FE_ClearFileReadSelect (MWContext *context, int fd)
{
  FE_ClearReadSelect (context, fd);
}

void
FE_SetFileReadSelect (MWContext *context, int fd)
{
  FE_SetReadSelect (context, fd);
}


/* making contexts and windows */

static unsigned int
default_char_width (int charset, fe_Font font)
{
  XCharStruct overall;
  int ascent, descent;
  FE_TEXT_EXTENTS(charset, font, "n", 1, &ascent, &descent, &overall);
  return overall.width;
}


static void fe_pick_visual_and_colormap (Widget toplevel,
					 MWContext *new_context);
void fe_get_context_resources (MWContext *context);
void fe_get_final_context_resources (MWContext *context);

MWContext *
fe_MakeWindow(Widget toplevel, MWContext *context_to_copy,
			URL_Struct *url, char *window_name, MWContextType type,
			Boolean skip_get_url)
{
    return fe_MakeNewWindow(toplevel, context_to_copy, url, window_name, type,
			    skip_get_url, NULL);
}

static void
fe_position_download_context(MWContext *context)
{
  MWContext *active_context = NULL;
  Widget shell = CONTEXT_WIDGET(context);

  XP_ASSERT(context->type == MWContextSaveToDisk);

  /* Lets position this ourselves. If window manager interactive placement
     is on and this context doesn't exist for more than a few microsecs,
     then the user would see this as a outline, place it somewhere but
     wouldn't see a window at all.
  */
  if (fe_all_MWContexts->next)
    active_context = fe_all_MWContexts->next->context;
  if (active_context) {
    WMShellWidget wmshell = (WMShellWidget) shell;
    Widget parent = CONTEXT_WIDGET(active_context);
    Screen* screen = XtScreen (parent);
    Dimension screen_width = WidthOfScreen (screen);
    Dimension screen_height = HeightOfScreen (screen);
    Dimension parent_width = 0;
    Dimension parent_height = 0;
    Dimension child_width = 0;
    Dimension child_height = 0;
    Position x;
    Position y;
    XSizeHints size_hints;

    XtRealizeWidget (shell);  /* to cause its size to be computed */

    XtVaGetValues(shell,
		      XtNwidth, &child_width, XtNheight, &child_height, 0);
    XtVaGetValues (parent,
		       XtNwidth, &parent_width, XtNheight, &parent_height, 0);

    x = (((Position)parent_width) - ((Position)child_width)) / 2;
    y = (((Position)parent_height) - ((Position)child_height)) / 2;
    XtTranslateCoords (parent, x, y, &x, &y);

    if ((Dimension) (x + child_width) > screen_width)
      x = screen_width - child_width;
    if (x < 0)
      x = 0;

    if ((Dimension) (y + child_height) > screen_height)
      y = screen_height - child_height;
    if (y < 0)
      y = 0;

    XtVaSetValues (shell, XtNx, x, XtNy, y, 0);

    /* Horrific kludge */
    wmshell->wm.size_hints.flags &= (~PPosition);
    wmshell->wm.size_hints.flags |= USPosition;
    if (XGetNormalHints (XtDisplay(shell), XtWindow(shell), &size_hints)) {
      size_hints.x = wmshell->wm.size_hints.x;
      size_hints.y = wmshell->wm.size_hints.y;
      size_hints.flags &= (~PPosition);
      size_hints.flags |= USPosition;
      XSetNormalHints (XtDisplay(shell), XtWindow(shell), &size_hints);
    }
  }
}

void
fe_find_scrollbar_sizes(MWContext *context)
{
	/* Figure out how much space the horizontal & vertical scrollbars take up.
	 * It's basically impossible to determine this before creating them...
	 */
	if (CONTEXT_DATA(context)->scrolled) {
		Boolean   hscrollP = XtIsManaged(CONTEXT_DATA (context)->hscroll);
		Boolean   vscrollP = XtIsManaged(CONTEXT_DATA (context)->vscroll);
		Dimension w1 = 0, w2 = 0;
		Dimension h1 = 0, h2 = 0;
		
		XtVaGetValues (CONTEXT_DATA (context)->drawing_area, 
					   XmNwidth, &w2, 
					   XmNheight, &h2, 
					   0);

		/* Vertical ScrollBar dimensions...
		 * 
		 */
		if (!fe_SBW_P) {
			XtVaGetValues (CONTEXT_DATA (context)->scrolled, 
						   XmNwidth, &w1, 0);
			if (vscrollP) {
				fe_ScrollBarW = w1 - w2;
				fe_SBW_P = True;
			}
			else {
				/*
				 * MAGIC Number... [ default value ]
				 */
				fe_ScrollBarW = 15;
			}
		}
		CONTEXT_DATA (context)->sb_w = fe_ScrollBarW;
		CONTEXT_DATA (context)->scrolled_width = (unsigned long) w2;

		/* Horizontal ScrollBar dimensions...
		 * 
		 */
		if (!fe_SBH_P) {
			if (hscrollP) {
				XtVaGetValues (CONTEXT_DATA (context)->scrolled, 
							   XmNheight, &h1, 0);
				fe_ScrollBarH = h1 - h2;
				fe_SBH_P = True;
			}
			else {
				/*
				 * MAGIC Number... [ default value ]
				 */
				fe_ScrollBarH = 15;
				}
		}
		CONTEXT_DATA (context)->sb_h = fe_ScrollBarH;
		CONTEXT_DATA (context)->scrolled_height = (unsigned long) h2;
	}
}


void
fe_load_default_font(MWContext *context)
{
  fe_Font font;
  int ascent;
  int descent;
  int16 charset;

  charset = CS_LATIN1;
  font = fe_LoadFontFromFace(context, NULL, &charset, 0, 3, 0);
  if (!font)
  {
	CONTEXT_DATA (context)->line_height = 17;
	return;
  }
  FE_FONT_EXTENTS(CS_LATIN1, font, &ascent, &descent);
  CONTEXT_DATA (context)->line_height = ascent + descent;
  fe_DoneWithFont(font);
}

static void
fe_focus_notify_eh (Widget w, XtPointer closure, XEvent *ev, Boolean *cont)
{
  MWContext *context = (MWContext *) closure;
  JSEvent *event;

  TRACEMSG (("fe_focus_notify_eh\n"));
  switch (ev->type) {
  case FocusIn:
    TRACEMSG (("focus in\n"));
    fe_MochaFocusNotify(context, NULL);
    break;
  case FocusOut:
    TRACEMSG (("focus out\n"));
    fe_MochaBlurNotify(context, NULL);
    break;
  }
}

void
fe_map_notify_eh (Widget w, XtPointer closure, XEvent *ev, Boolean *cont)
{
#ifdef JAVA
    MWContext *context = (MWContext *) closure;
    switch (ev->type) {
    case MapNotify:
	LJ_UniconifyApplets(context);
	break;
    case UnmapNotify:
	LJ_IconifyApplets(context);
	break;
    }
#endif /* JAVA */
}

void
fe_copy_context_settings(MWContext *to, MWContext *from)
{
  /* notyet fe_ContextData* dto; */

  if ((NULL==to) || (NULL==from))
	return;

  /* set default doc_csid */
  if ((NULL!=to->fe.data) && (NULL!=from->fe.data)) {
      to->fe.data->xfe_doc_csid = from->fe.data->xfe_doc_csid;
  }

#ifdef notyet
  dto = CONTEXT_DATA(to);
  if (from) {
    fe_ContextData* dfrom = CONTEXT_DATA(from);
    dto->show_url_p		= dfrom->show_url_p;
    dto->show_toolbar_p		= dfrom->show_toolbar_p;
    dto->show_toolbar_icons_p	= dfrom->show_toolbar_icons_p;
    dto->show_toolbar_text_p	= dfrom->show_toolbar_text_p;
#ifdef EDITOR
    dto->show_character_toolbar_p = dfrom->show_character_toolbar_p;
    dto->show_paragraph_toolbar_p = dfrom->show_paragraph_toolbar_p;
#endif
    dto->show_directory_buttons_p = dfrom->show_directory_buttons_p;
    dto->show_menubar_p           = dfrom->show_menubar_p;
    dto->show_bottom_status_bar_p = dfrom->show_bottom_status_bar_p;
#ifndef NO_SECURITY
    dto->show_security_bar_p	= dfrom->show_security_bar_p;
#endif
    dto->autoload_images_p	= dfrom->autoload_images_p;
    dto->loading_images_p	= False;
    dto->looping_images_p	= False;
    dto->delayed_images_p	= dfrom->delayed_images_p;
    dto->force_load_images	= 0;
    dto->fancy_ftp_p		= dfrom->fancy_ftp_p;
    dto->xfe_doc_csid		= dfrom->xfe_doc_csid;
    dto->XpixelsPerPoint        = dfrom->XpixelsPerPoint;
    dto->YpixelsPerPoint        = dfrom->YpixelsPerPoint;
  }
  else {
    dto->show_url_p		= fe_globalPrefs.show_url_p;
    dto->show_toolbar_p		= fe_globalPrefs.show_toolbar_p;
#ifdef EDITOR
    dto->show_character_toolbar_p = fe_globalPrefs.editor_character_toolbar;
    dto->show_paragraph_toolbar_p = fe_globalPrefs.editor_paragraph_toolbar;
#endif
    dto->show_toolbar_icons_p	= fe_globalPrefs.toolbar_icons_p;
    dto->show_toolbar_text_p	= fe_globalPrefs.toolbar_text_p;
    dto->show_directory_buttons_p = fe_globalPrefs.show_directory_buttons_p;
    dto->show_menubar_p           = fe_globalPrefs.show_menubar_p;
    dto->show_bottom_status_bar_p = fe_globalPrefs.show_bottom_status_bar_p;
#ifndef NO_SECURITY
    dto->show_security_bar_p	= fe_globalPrefs.show_security_bar_p;
#endif
    dto->autoload_images_p	= fe_globalPrefs.autoload_images_p;
    dto->loading_images_p	= False;
    dto->looping_images_p	= False;
    dto->delayed_images_p	= False;
    dto->force_load_images	= 0;
    dto->fancy_ftp_p		= fe_globalPrefs.fancy_ftp_p;
    dto->xfe_doc_csid		= INTL_DefaultDocCharSetID(NULL);
    /* XpixelsPerPoint and YpixelsPerPoint are caculated in 
     * XFE_Frame::initializeMWContext.
     */
  }
#endif /* notyet */

}

/* #### mailnews.c */
extern void fe_set_compose_wrap_state(MWContext *context, XP_Bool wrap_p);

/* XXX - "temporary" routine until we pass fe_drawable's straight from
   layout to FE drawing functions */
void
XFE_SetDrawable(MWContext *context, CL_Drawable *drawable)
{ 
  fe_Drawable *fe_drawable;
  if (! drawable)
    return;

  fe_drawable = (fe_Drawable*)CL_GetDrawableClientData(drawable);
  CONTEXT_DATA(context)->drawable = fe_drawable;
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

/* XXX - Works faster if we don't define this, but seems to introduce bugs */
#define USE_REGION_FOR_COPY

#ifndef USE_REGION_FOR_COPY

static Drawable fe_copy_src, fe_copy_dst;
static GC fe_copy_gc;

static void
fe_copy_rect_func(void *empty, XP_Rect *rect)
{
  XCopyArea (fe_display, fe_copy_src, fe_copy_dst, fe_copy_gc,
	     rect->left, rect->top,
	     rect->right - rect->left, rect->bottom - rect->top,
	     rect->left, rect->top);
}
#endif

static void
fe_copy_pixels(CL_Drawable *drawable_src, 
               CL_Drawable *drawable_dst, 
               FE_Region region)
{
  XP_Rect bbox;
  XGCValues gcv;
  GC gc;
  fe_Drawable *fe_drawable_dst = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable_dst);
  fe_Drawable *fe_drawable_src = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable_src);

  Drawable src = fe_drawable_src->xdrawable;
  Drawable dst = fe_drawable_dst->xdrawable;

  /* FIXME - for simple regions, it may be faster to iterate over
     rectangles rather than using clipping regions */ 
  memset (&gcv, ~0, sizeof (gcv));
  gcv.function = GXcopy;

#ifdef USE_REGION_FOR_COPY
  gc = fe_GetGCfromDW (fe_display, dst, GCFunction, &gcv, region);

  FE_GetRegionBoundingBox(region, &bbox);
  XCopyArea (fe_display, src, dst, gc, bbox.left, bbox.top,
	     bbox.right - bbox.left, bbox.bottom - bbox.top,
	     bbox.left, bbox.top);
#else
  fe_copy_gc = fe_GetGCfromDW (fe_display, dst, GCFunction, &gcv, NULL);
  fe_copy_src = src;
  fe_copy_dst = dst;
  FE_ForEachRectInRegion(region, 
			 (FE_RectInRegionFunc)fe_copy_rect_func, NULL);
#endif
}

/* There is only one backing store pixmap shared among all windows */
static Pixmap fe_backing_store_pixmap = 0;

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
  XFreePixmap(fe_display, fe_backing_store_pixmap);
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
    if (fe_drawable->xdrawable_serial_num !=
        fe_backing_store_pixmap_serial_num) {
        return PR_FALSE;
    }
  }

  backing_store_owner = fe_drawable;

  fe_drawable->xdrawable = fe_backing_store_pixmap;
  fe_drawable->xdrawable_serial_num = fe_backing_store_pixmap_serial_num;

  return PR_TRUE;
}

static void
fe_set_drawable_dimensions(CL_Drawable *drawable, uint32 width, uint32 height)
{
  Window backing_store_window;
  struct fe_MWContext_cons *cntx;
  fe_Drawable *fe_drawable = 
    (fe_Drawable*)CL_GetDrawableClientData(drawable);

  XP_ASSERT(backing_store_refcount > 0);
  if ((width > backing_store_width) || (height > backing_store_height)) {  
        
    /* The backing store only gets larger, not smaller. */
    if (width < backing_store_width)
      width = backing_store_width;
    if (height < backing_store_height)
      height = backing_store_height;
        
    if (fe_backing_store_pixmap) {
      XFreePixmap(fe_display, fe_backing_store_pixmap);
      backing_store_owner = NULL;
    }

    fe_backing_store_pixmap_serial_num++;

    /* Grab a window for use with XCreatePixmap.  X doesn't care about
       what window we use, as long as it's on the same screen that the
       pixmap will be drawn on. */
    backing_store_window = 0;
    for (cntx = fe_all_MWContexts; cntx; cntx = cntx->next) {
        if (CONTEXT_DATA (cntx->context)->drawing_area != NULL) {
            backing_store_window = 
                XtWindow (CONTEXT_DATA (cntx->context)->drawing_area);
            if (backing_store_window)
                break;
        }
    }
    if (!backing_store_window) {
        XP_ASSERT(0);
        return;
    }

    fe_backing_store_pixmap = XCreatePixmap (fe_display, 
					     backing_store_window,
					     width, height,
					     backing_store_depth);
    backing_store_width = width;
    backing_store_height = height;
  }
  fe_drawable->xdrawable = fe_backing_store_pixmap;
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
fe_create_compositor(MWContext *context)
{
  int32 comp_width, comp_height;
  CL_Drawable *cl_window_drawable, *cl_backing_store_drawable;
  CL_Compositor *compositor;
  fe_Drawable *window_drawable, *backing_store_drawable;
  Window window;

  Visual *visual;

  window = XtWindow(CONTEXT_DATA(context)->drawing_area);
  visual = fe_globalData.default_visual;
  backing_store_depth = fe_VisualDepth (fe_display, visual);

  /* Create a new compositor and its default layers */
  comp_width = CONTEXT_DATA(context)->scrolled_width;
  comp_height = CONTEXT_DATA(context)->scrolled_height;

  if (CONTEXT_DATA(context)->vscroll && 
      XtIsManaged(CONTEXT_DATA(context)->vscroll))
    comp_width -= CONTEXT_DATA(context)->sb_w;
  if (CONTEXT_DATA(context)->hscroll &&
      XtIsManaged(CONTEXT_DATA(context)->hscroll))
    comp_height -= CONTEXT_DATA(context)->sb_h;

  window_drawable = XP_NEW_ZAP(fe_Drawable);
  if (!window_drawable)
    return NULL;
  window_drawable->xdrawable = window;

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

  /* Set reasonable default drawable */
  CONTEXT_DATA (context)->drawable = window_drawable;

  return compositor;
}

/* Create and initialize the Image Library JMC callback interface.
   Also create an IL_GroupContext for the current context. */
XP_Bool
fe_init_image_callbacks(MWContext *context)
{
    IL_GroupContext *img_cx;
    IMGCB* img_cb;
    JMCException *exc = NULL;

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
        observer_added_p = IL_AddGroupObserver(img_cx, fe_ImageGroupObserver,
                                               (void *)context);
    }
    return TRUE;
}

/* in src/context_funcs.cpp */
extern MWContext *xfe2_MakeNewWindow(Widget, MWContext*, URL_Struct*,
									 char *,MWContextType, Boolean, Chrome*);

MWContext *
fe_MakeNewWindow(Widget toplevel, MWContext *context_to_copy,
			URL_Struct *url, char *window_name, MWContextType type,
			Boolean skip_get_url, Chrome *decor)
{
  MWContext* context;
  struct fe_MWContext_cons *cons;
  fe_ContextData *fec;
  Widget shell;
  char *shell_name;

  int delete_response;
  Boolean allow_resize;
  Boolean is_modal;
  Boolean context_displays_html_p = False;
  Arg av[20];
  int ac;
  int16 new_charset;

  new_charset = CS_DEFAULT;

  /* Fix type */
  if (url && (type != MWContextSaveToDisk) && (type != MWContextBookmarks) &&
	(type != MWContextAddressBook) && (type != MWContextDialog) &&
        (type != MWContextEditor)) {
#ifdef MOZ_MAIL_NEWS
    if (MSG_RequiresMailWindow (url->address) || MSG_RequiresNewsWindow(url->address))
      type = MWContextMail;
    else if (MSG_RequiresBrowserWindow (url->address)) {
#else
    {
#endif
	type = MWContextBrowser;
    }
  }

  return xfe2_MakeNewWindow(toplevel, context_to_copy, url, window_name,
							type, skip_get_url, decor);
}


MWContext *
FE_MakeNewWindow(MWContext  *old_context,
		 URL_Struct *url,
		 char       *window_name,
		 Chrome     *chrome)
{
  MWContext *   new_context;
  MWContextType type = MWContextBrowser;

  if ( (old_context == NULL)
       || (CONTEXT_WIDGET (old_context) == NULL) )
    return(NULL);

  if (chrome && chrome->type != 0)
    type = chrome->type;

  /*
   * Dependent Windows
   */
  if (chrome && chrome->dependent)
    {
      /*
       * Ensure that a dependent list exists, returning NULL
       * if one cannot be created.
       */
      if (old_context->js_dependent_list == 0)
	{
	  XP_List * list;
	  list = XP_ListNew();
	  if (!list)
	    return NULL;
	  old_context->js_dependent_list = list;
	}
    }

  new_context = fe_MakeNewWindow (XtParent(CONTEXT_WIDGET (old_context)),
				  old_context, url,
				  window_name, type, (url == NULL), chrome);

  if (chrome && chrome->dependent)
    {
      /* parent knows about dependent */
      XP_ListAddObject(old_context->js_dependent_list, new_context);
      /* dependent knows about parent */
      new_context->js_parent = old_context;
    }

  return(new_context);
}

MWContext *
FE_MakeBlankWindow(MWContext *old_context, URL_Struct *url, char *window_name)
{
	MWContext *new_context;

	if ((old_context == NULL)||(CONTEXT_WIDGET (old_context) == NULL))
	{
		return(NULL);
	}

	new_context = fe_MakeWindow (
		XtParent(CONTEXT_WIDGET (old_context)),
		old_context, NULL, window_name, MWContextBrowser, TRUE);
	return(new_context);
}


void
FE_SetWindowLoading(MWContext *context, URL_Struct *url,
	Net_GetUrlExitFunc **exit_func_p)
{
	if ((context != NULL)&&(url != NULL))
	{
		fe_GetURL (context, url, TRUE);
		*exit_func_p = (Net_GetUrlExitFunc *)fe_url_exit;
	}
}


Boolean
fe_IsGridParent (MWContext *context)
{
  return (context->grid_children && XP_ListTopObject (context->grid_children));
}


MWContext *
fe_GetFocusGridOfContext(MWContext *context)
{
  MWContext *child;
  int i = 1;

  if (context == NULL)
    return 0;

  /* grid_children keeps a list of html frames...
     it's possible that it is null when there is no html frames created */
  if ( !context->grid_children) return 0;

  while ((child = (MWContext*)XP_ListGetObjectNum (context->grid_children,
						   i++))) {
    if (CONTEXT_DATA (child)->focus_grid)
      return child;

    if (fe_IsGridParent (child)) {
      child = fe_GetFocusGridOfContext (child);
      if (child != NULL)
	return child;
    }
  }
  return 0;
}

void
fe_MochaFocusNotify (MWContext *context, LO_Element *element)
{
    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_FOCUS;
    ET_SendEvent (context, element, event, NULL, NULL);
}

void
fe_MochaBlurNotify (MWContext *context, LO_Element *element)
{
    JSEvent *event;
    event = XP_NEW_ZAP(JSEvent);
    event->type = EVENT_BLUR;
    ET_SendEvent (context, element, event, NULL, NULL);
}

void
fe_SetGridFocus (MWContext *context)
{
  Widget w;
  MWContext *top, *focus_grid;
  Dimension border_width;

  if (context == NULL)
    return;

  /* focus the new guy */
  fe_MochaFocusNotify (context, NULL);

  top = XP_GetNonGridContext (context);
  if (top == NULL)
    return;

  if ((focus_grid = fe_GetFocusGridOfContext (top))) {
    /* blur the previous guy */
    fe_MochaBlurNotify (focus_grid, NULL);

    CONTEXT_DATA (focus_grid)->focus_grid = False;
    w = CONTEXT_DATA (focus_grid)->main_pane;
    XtVaGetValues (w, XmNborderWidth, &border_width, 0);
    if (border_width)
      XtVaSetValues (w, XmNborderColor, 
		     (CONTEXT_DATA (context)->default_bg_pixel), 0);
  }

  /* Then indicate which cell has focus */
  TRACEMSG (("context: 0x%x has focus\n", context));

  CONTEXT_DATA (context)->focus_grid = True;
  w = CONTEXT_DATA (context)->main_pane;

  XtVaGetValues (w, XmNborderWidth, &border_width, 0);
  if (border_width)
    XtVaSetValues (w, XmNborderColor,
		   CONTEXT_DATA (context)->default_fg_pixel, 0);

  XFE_SetDocTitle (context, 0);
}

MWContext *
#ifdef XP_UNIX
FE_MakeGridWindow (MWContext *old_context, void *hist_list, void *history,
	int32 x, int32 y,
#else
FE_MakeGridWindow (MWContext *old_context, void *history, int32 x, int32 y,
#endif /* XP_UNIX */
        int32 width, int32 height, char *url_str, char *window_name,
	int8 scrolling, NET_ReloadMethod force_reload, Bool no_edge)
{
  Widget parent = CONTEXT_DATA (old_context)->drawing_area;
  MWContext *context = XP_NewContext();
  struct fe_MWContext_cons *cons = (struct fe_MWContext_cons *)
    malloc (sizeof (struct fe_MWContext_cons));
  fe_ContextData *fec = (fe_ContextData *) calloc (sizeof (fe_ContextData), 1);
  History_entry *he = (History_entry *)history;
  URL_Struct *url = NULL;

  CONTEXT_DATA (context) = fec;

  /* add the layout function pointers
   */
  context->funcs = fe_BuildDisplayFunctionTable();
  context->convertPixX = context->convertPixY = 1;
  context->is_grid_cell = TRUE;
  context->grid_parent = old_context;

  /* New field added by putterman for increase/decrease font */
  context->fontScalingPercentage = old_context->fontScalingPercentage;

  cons->context = context;
  cons->next = fe_all_MWContexts;
  fe_all_MWContexts = cons;

  /* pixelsPerPoint:  display-specific information needed by the back end
   * when converting style sheet length units between points and pixels.
   */
  context->XpixelsPerPoint = old_context->XpixelsPerPoint;
  context->YpixelsPerPoint = old_context->YpixelsPerPoint;

  SHIST_InitSession (context);		/* Initialize the history library. */
#ifdef XP_UNIX
  if (hist_list != NULL)
  {
    context->hist.list_ptr = hist_list;
  }
  else
  {
    SHIST_AddDocument(context, he);
  }
#else
  SHIST_AddDocument(context, he);
#endif /* XP_UNIX */

  if (he)
    url = SHIST_CreateURLStructFromHistoryEntry (context, he);
  else
    url = NET_CreateURLStruct (url_str, NET_DONT_RELOAD);

  if (url) {
    MWContext *top = XP_GetNonGridContext(old_context);
    History_entry *h = SHIST_GetCurrent (&top->hist);

    url->force_reload = force_reload;

    /* Set the referer field in the url to the document that refered to the
     * grid parent.  New function fe_GetURLForReferral() might be used
     * here, brendan says this is probably Ok. -mcafee
     */
    if (h && h->referer)
      url->referer = strdup(h->referer);
  }

  if (window_name)
    {
      context->name = strdup (window_name);
    }
  XP_AddContextToList (context);

  if (old_context)
    {
      CONTEXT_DATA (context)->autoload_images_p =
        CONTEXT_DATA (old_context)->autoload_images_p;
      CONTEXT_DATA (context)->loading_images_p = False;
      CONTEXT_DATA (context)->looping_images_p = False;
      CONTEXT_DATA (context)->delayed_images_p =
        CONTEXT_DATA (old_context)->delayed_images_p;
      CONTEXT_DATA (context)->force_load_images = 0;
      CONTEXT_DATA (context)->fancy_ftp_p =
        CONTEXT_DATA (old_context)->fancy_ftp_p;
      CONTEXT_DATA (context)->xfe_doc_csid =
        CONTEXT_DATA (old_context)->xfe_doc_csid;
    }
  CONTEXT_WIDGET (context) = CONTEXT_WIDGET (old_context);
  CONTEXT_DATA (context)->backdrop_pixmap = (Pixmap) ~0;
  CONTEXT_DATA (context)->grid_scrolling = scrolling;

  /* FRAMES_HAVE_THEIR_OWN_COLORMAP was an unfinished
	 experiment by kevina. */
#ifdef FRAMES_HAVE_THEIR_OWN_COLORMAP
  /* We have to go through this to get the toplevel widget */
  {
      MWContext *top_context = XP_GetNonGridContext(context);

      fe_pick_visual_and_colormap (XtParent(CONTEXT_WIDGET (top_context)),
                                   context);
  }
#else
  /* Inherit colormap from our parent */
  CONTEXT_DATA(context)->colormap = CONTEXT_DATA(old_context)->colormap;
#endif

#ifdef FRAMES_HAVE_THEIR_OWN_COLORMAP
        /* XXXM12N Create and initialize the Image Library JMC callback
           interface.  Also create a new IL_GroupContext for this window.*/
        if (!fe_init_image_callbacks(context))
            {
                return NULL;
            }
  fe_InitColormap(context);
#endif

  XtGetApplicationResources (CONTEXT_WIDGET (old_context),
                             (XtPointer) CONTEXT_DATA (context),
                             fe_Resources, fe_ResourcesSize,
                             0, 0);

/* CONTEXT_DATA (context)->main_pane = parent; */

  /*
   * set the default coloring correctly into the new context.
   */
  {
    Pixel unused_select_pixel;
    XmGetColors (XtScreen (parent),
		 fe_cmap(context),
		 CONTEXT_DATA (context)->default_bg_pixel,
		 &(CONTEXT_DATA (context)->fg_pixel),
		 &(CONTEXT_DATA (context)->top_shadow_pixel),
		 &(CONTEXT_DATA (context)->bottom_shadow_pixel),
		 &unused_select_pixel);
  }

  /* ### Create a form widget to parent the scroller.
   *
   * This might keep the scroller from becoming smaller than
   * the cell size when there are no scrollbars.
   */

  {
    Arg av [50];
    int ac;
    Widget pane, mainw, scroller;
    int border_width = 0;

    if (no_edge)
      border_width = 0;
    else
      border_width = 2;

    ac = 0;
    XtSetArg (av[ac], XmNx, (Position)x); ac++;
    XtSetArg (av[ac], XmNy, (Position)y); ac++;
    XtSetArg (av[ac], XmNwidth, (Dimension)width - 2*border_width); ac++;
    XtSetArg (av[ac], XmNheight, (Dimension)height - 2*border_width); ac++;
    XtSetArg (av[ac], XmNborderWidth, border_width); ac++;
    mainw = XmCreateForm (parent, "form", av, ac);

    ac = 0;
    XtSetArg (av[ac], XmNborderWidth, 0); ac++;
    XtSetArg (av[ac], XmNmarginWidth, 0); ac++;
    XtSetArg (av[ac], XmNmarginHeight, 0); ac++;
    XtSetArg (av[ac], XmNborderColor,
		      CONTEXT_DATA (context)->default_bg_pixel); ac++;
    pane = XmCreatePanedWindow (mainw, "pane", av, ac);

    XtVaSetValues (pane,
		   XmNtopAttachment, XmATTACH_FORM,
		   XmNbottomAttachment, XmATTACH_FORM,
		   XmNleftAttachment, XmATTACH_FORM,
		   XmNrightAttachment, XmATTACH_FORM,
		   0);

    /* The actual work area */
    scroller = fe_MakeScrolledWindow (context, pane, "scroller");
    XtVaSetValues (CONTEXT_DATA (context)->scrolled,
		   XmNborderWidth, 0, 0);

    XtManageChild (scroller);
    XtManageChild (pane);
    XtManageChild (mainw);

    CONTEXT_DATA (context)->main_pane = mainw;
  }

  fe_load_default_font (context);
  fe_get_context_resources (context);   /* Do other resource db hackery. */

  /* FIXME - This is flagrantly wasteful of backing store memory.
     Contexts which are not leaves in the FRAMESET hierarchy don't
     need any backing store or compositor. */
  context->compositor = fe_create_compositor(context);

  /* Figure out how much space the horizontal and vertical scrollbars take up.
     It's basically impossible to determine this before creating them...
   */
  {
    Dimension w1 = 0, w2 = 0, h1 = 0, h2 = 0;

    XtManageChild (CONTEXT_DATA (context)->hscroll);
    XtManageChild (CONTEXT_DATA (context)->vscroll);
    XtVaGetValues (CONTEXT_DATA (context)->drawing_area,
                                 XmNwidth, &w1,
                                 XmNheight, &h1,
                                 0);

    XtUnmanageChild (CONTEXT_DATA (context)->hscroll);
    XtUnmanageChild (CONTEXT_DATA (context)->vscroll);
    XtVaGetValues (CONTEXT_DATA (context)->drawing_area,
                                 XmNwidth, &w2,
                                 XmNheight, &h2,
                                 0);

    CONTEXT_DATA (context)->sb_w = w2 - w1;
    CONTEXT_DATA (context)->sb_h = h2 - h1;

    /* Now that we know, we don't need to leave them managed. */
  }

  XtVaSetValues (CONTEXT_DATA (context)->scrolled, XmNinitialFocus,
                 CONTEXT_DATA (context)->drawing_area, 0);

  fe_SetGridFocus (context);  /* Give this grid focus */
  fe_InitScrolling (context); /* big voodoo */

  /* XXXM12N Create and initialize the Image Library JMC callback
     interface.  Also create a new IL_GroupContext for this window.*/
  if (!context->img_cx)
      if (!fe_init_image_callbacks(context))
          {
              return NULL;
          }

  fe_InitColormap (context);

  if (url)
    {
      /* #### This might not be right, or there might be more that needs
         to be done...   Note that url->history_num is bogus for the new
         context.  url->position_tag might also be context-specific. */
#ifdef XP_UNIX
      /*
       * I believe that if we are restoring from a history entry,
       * we don't want to clear this saved data.
       */
      if (!he)
      {
        XP_MEMSET (&url->savedData, 0, sizeof (SHIST_SavedData));
      }
#else
      XP_MEMSET (&url->savedData, 0, sizeof (SHIST_SavedData));
#endif /* XP_UNIX */
      fe_GetURL (context, url, FALSE);
    }

  XFE_SetDocTitle (context, 0);
  CONTEXT_DATA (context)->are_scrollbars_active = True;

  return(context);
}

void
FE_RestructureGridWindow (MWContext *context, int32 x, int32 y,
        int32 width, int32 height)
{
  Widget mainw;

  /*
   * This comes from blank frames.  Maybe we should clear them
   * before we return?
   */
  if (!context ) return;

  /*
   * Basically we just set the new position and dimensions onto the
   * parent of the drawing area.  X will do the rest for us.
   * Because of the window gravity side effects of guffaws scrolling
   * The drawing area won't move with its parent unless we temporarily
   * turn off guffaws.
   */
  mainw = CONTEXT_DATA (context)->main_pane;
  fe_SetGuffaw(context, FALSE);
  XtVaSetValues (mainw,
		 XmNx, (Position)x,
		 XmNy, (Position)y,
		 XmNwidth, (Dimension)width - 4,   /* Adjust for focus border */
		 XmNheight, (Dimension)height - 4,
		 0);
  fe_SetGuffaw(context, TRUE);
}

/* Required to make custom colormap installation work.
   Really, not *every* context has a private colormap.
   Trivial contexts like address book, bookmarks, etc.
   share a colormap. */
#define EVERY_CONTEXT_HAS_PRIVATE_COLORMAP

static void
fe_pick_visual_and_colormap (Widget toplevel, MWContext *new_context)
{
  Screen *screen = XtScreen (toplevel);
  Display *dpy = XtDisplay (toplevel);
  Colormap cmap;
  Visual *v;
  fe_colormap *colormap;

  v = fe_globalData.default_visual;
  if (!v)
  {
    String str = 0;
    /* "*visualID" is special for a number of reasons... */
    static XtResource res = { "visualID", "VisualID",
			      XtRString, sizeof (String),
			      0, XtRString, "default" };
    XtGetSubresources (toplevel, &str, (char *) fe_progname, "TopLevelShell",
		       &res, 1, 0, 0);
    v = fe_ParseVisual (screen, str);
    fe_globalData.default_visual = v;
  }

  {
    String str = 0;
    static XtResource res = { "installColormap", XtCString, XtRString,
			      sizeof (String), 0, XtRString, "guess" };
    XtGetApplicationResources (toplevel, &str, &res, 1, 0, 0);
    if (!str || !*str || !XP_STRCASECMP(str, "guess"))
      {
	/* But everybody lies about this value */
	char *vendor = XServerVendor (XtDisplay (toplevel));
	fe_globalData.always_install_cmap =
	  !strcmp (vendor, "Silicon Graphics");
      }
    else if (!XP_STRCASECMP(str, "yes") || !XP_STRCASECMP(str, "true"))
      fe_globalData.always_install_cmap = True;
    else if (!XP_STRCASECMP(str, "no") || !XP_STRCASECMP(str, "false"))
      fe_globalData.always_install_cmap = False;
    else
      {
	fprintf (stderr,
		 XP_GetString(XFE_DISPLAY_FACTORY_INSTALL_COLORMAP_ERROR),
		 fe_progname, str);
	fe_globalData.always_install_cmap = False;
      }
  }

  /* Don't allow colormap flashing on a deep display */
  if (v != DefaultVisualOfScreen (screen))
    fe_globalData.always_install_cmap = True;
  
  if (!fe_globalData.default_colormap)
    {
      cmap = DefaultColormapOfScreen (screen);
      fe_globalData.default_colormap =
          fe_NewColormap(screen,  DefaultVisualOfScreen (screen), cmap, False);
    }

  colormap = fe_globalData.common_colormap;

  if (!colormap)
    {
      if (fe_globalData.always_install_cmap)
        {
          /* Create a colormap for "simple" contexts 
             like bookmarks, address book, etc */
          cmap = XCreateColormap (dpy, RootWindowOfScreen (screen),
                                  v, AllocNone);
          colormap = fe_NewColormap(screen, v, cmap, True);
        }
      else
        {
          /* Use the default colormap for all contexts. */
          colormap = fe_globalData.default_colormap;
        }
      fe_globalData.common_colormap = colormap;
    }
  
      
#ifdef EVERY_CONTEXT_HAS_PRIVATE_COLORMAP
  if (fe_globalData.always_install_cmap)
    {
      /* Even when installing "private" colormaps for every window,
         "simple" contexts, which have fixed color composition, share
         a single colormap. */
      MWContextType type = new_context->type;
      if ((type == MWContextBrowser) ||
          (type == MWContextEditor)  ||
          (type == MWContextNews)    ||
          (type == MWContextMail))
        {
          cmap = XCreateColormap (dpy, RootWindowOfScreen (screen),
                                  v, AllocNone);
          colormap = fe_NewColormap(screen, v, cmap, True);
        }
    }
#endif  /* !EVERY_CONTEXT_HAS_PRIVATE_COLORMAP */

  CONTEXT_DATA (new_context)->colormap = colormap;
}

void
fe_InitializeGlobalResources (Widget toplevel)
{
  XtGetApplicationResources (toplevel,
			     (XtPointer) &fe_globalData,
			     fe_GlobalResources, fe_GlobalResourcesSize,
			     0, 0);

  /*
   *    And then there was Sun. Try to detect losing olwm,
   *    and default to mono desktop icons.
   */
  if (fe_globalData.wm_icon_policy == NULL) { /* not set */
      if (XfeIsOpenLookRunning(toplevel))
	  fe_globalData.wm_icon_policy = "mono";
      else
	  fe_globalData.wm_icon_policy = "color";
  }
    
  /* Add a timer to periodically flush out the global history and bookmark. */
  fe_save_history_timer ((XtPointer) ((int) True), 0);

  /* #### move to prefs */
  LO_SetUserOverride (!fe_globalData.document_beats_user_p);
}


/* This initializes resources which must be set up BEFORE the widget is
   realized or managed (sizes and things). */
void
fe_get_context_resources (MWContext *context)
{
  fe_ContextData *fec = CONTEXT_DATA (context);
  if (fec->drawing_area) {
    XtVaGetValues (fec->drawing_area,
		   XmNbackground, &fec->bg_pixel, 0);
  } /* else??? ### */

  /* If the selection colors ended up mapping to the same pixel values,
     invert them. */
  if (CONTEXT_DATA (context)->select_fg_pixel ==
      CONTEXT_DATA (context)->fg_pixel &&
      CONTEXT_DATA (context)->select_bg_pixel ==
      CONTEXT_DATA (context)->bg_pixel)
    {
      CONTEXT_DATA (context)->select_fg_pixel =
	CONTEXT_DATA (context)->bg_pixel;
      CONTEXT_DATA (context)->select_bg_pixel =
	CONTEXT_DATA (context)->fg_pixel;
    }

  /* Tell layout about the default colors and background.
   */
  {
    XColor c[5];
    c[0].pixel = CONTEXT_DATA (context)->link_pixel;
    c[1].pixel = CONTEXT_DATA (context)->vlink_pixel;
    c[2].pixel = CONTEXT_DATA (context)->alink_pixel;
    c[3].pixel = CONTEXT_DATA (context)->default_fg_pixel;
    c[4].pixel = CONTEXT_DATA (context)->default_bg_pixel;
    XQueryColors (XtDisplay (CONTEXT_WIDGET (context)),
		  fe_cmap(context),
		  c, 5);
    LO_SetDefaultColor (LO_COLOR_LINK,
			c[0].red >> 8, c[0].green >> 8, c[0].blue >> 8);
    LO_SetDefaultColor (LO_COLOR_VLINK,
			c[1].red >> 8, c[1].green >> 8, c[1].blue >> 8);
    LO_SetDefaultColor (LO_COLOR_ALINK,
			c[2].red >> 8, c[2].green >> 8, c[2].blue >> 8);
    LO_SetDefaultColor (LO_COLOR_FG,
			c[3].red >> 8, c[3].green >> 8, c[3].blue >> 8);
    LO_SetDefaultColor (LO_COLOR_BG,
			c[4].red >> 8, c[4].green >> 8, c[4].blue >> 8);

    if (CONTEXT_DATA (context)->default_background_image &&
	*CONTEXT_DATA (context)->default_background_image)
      {
	char *bi = CONTEXT_DATA (context)->default_background_image;
	if (bi[0] == '/')
	  {
	    char *s = (char *) malloc (XP_STRLEN(bi) + 6);
	    strcpy (s, "file:");
	    strcat (s, bi);
	    LO_SetDefaultBackdrop (s);
	    free (s);
	  }
	else
	  {
	    LO_SetDefaultBackdrop (bi);
	  }
      }
  }
}


void
fe_set_scrolled_default_size(MWContext *context)
{
	/* Set the default size of the scrolling area based on the size of the
	   default font.  (This can be overridden by a -geometry argument.)
	   */
    int16 charset = CS_LATIN1;
    fe_Font font = fe_LoadFontFromFace(context, NULL, &charset, 0, 3, 0);
    Widget scrolled = XtNameToWidget (CONTEXT_WIDGET (context), "*scroller");
    unsigned int cw = (font ? default_char_width (CS_LATIN1, font) : 12);
    /* So just add 10% or so and hope it all fits. */
    Dimension w = (cw * 90);
	Dimension h = w;

	if (context->type == MWContextMessageComposition ) {
		/*
		 * NOTE:  let's try to pick a smaller MailCompose window...
		 */
		h = ( cw * 50 );
	}

    if (scrolled) {
		Dimension max_height = HeightOfScreen (XtScreen (scrolled));
		Dimension pseudo_max_height = 0;

		if (context->type == MWContextMessageComposition ) {
			pseudo_max_height = max_height * 0.50;
		}
		/* EDITOR & BROWSER...
		 *
		 * We don't want the default window size to be bigger than the screen.
		 * So don't make the default height of the scrolling area be more than
		 * 90% of the height of the screen.  This is pretty pseudo-nebulous,
		 * but again, getting exact numbers here is a royal pain.
		 */
		else {
			pseudo_max_height = max_height * 0.70;
		}

		if (h > pseudo_max_height)
			h = pseudo_max_height;

		XtVaSetValues (scrolled, XmNwidth, w, XmNheight, h, 0);
    }
  fe_DoneWithFont(font);
}


static void
fe_save_history_timer (XtPointer closure, XtIntervalId *id)
{
  Boolean init_only_p = (Boolean) ((int) closure);
  if (! init_only_p) {
    fe_SaveBookmarks ();
    GH_SaveGlobalHistory ();
    NET_WriteCacheFAT (0, False);
    NET_SaveCookies(NULL);
  }
  /* Re-add the timer. */
  fe_globalData.save_history_id =
    XtAppAddTimeOut (fe_XtAppContext,
		     fe_globalData.save_history_interval * 1000,
		     fe_save_history_timer, (XtPointer) ((int) False));
}


static void
fe_refresh_url_timer (XtPointer closure, XtIntervalId *id)
{
  MWContext *context = (MWContext *) closure;
  URL_Struct *url;

  CONTEXT_DATA (context)->refresh_url_timer = 0; /* clear */

  XP_ASSERT (CONTEXT_DATA (context)->refresh_url_timer_url);

  if (! CONTEXT_DATA (context)->refresh_url_timer_url)
    return;
  url = NET_CreateURLStruct (CONTEXT_DATA (context)->refresh_url_timer_url,
			     NET_NORMAL_RELOAD);
  url->force_reload = NET_NORMAL_RELOAD;
  fe_GetURL (context, url, FALSE);
}

void
FE_SetRefreshURLTimer (MWContext *context, uint32 secs, char *url)
{
  if(context->type != MWContextBrowser)
	return;

  if (CONTEXT_DATA (context)->refresh_url_timer)
    XtRemoveTimeOut (CONTEXT_DATA (context)->refresh_url_timer);
  if (CONTEXT_DATA (context)->refresh_url_timer_url)
    free (CONTEXT_DATA (context)->refresh_url_timer_url);
  CONTEXT_DATA (context)->refresh_url_timer = 0;
  CONTEXT_DATA (context)->refresh_url_timer_secs = secs;
  CONTEXT_DATA (context)->refresh_url_timer_url = strdup (url);
  if (secs <= 0)
    fe_refresh_url_timer ((XtPointer) context, 0);
  else
    CONTEXT_DATA (context)->refresh_url_timer =
      XtAppAddTimeOut (fe_XtAppContext, secs * 1000,
		       fe_refresh_url_timer, (XtPointer) context);
}


/* This initializes resources which must be set up AFTER the widget is
   realized (meaning we need a window of the correct depth.) */
void
fe_get_final_context_resources (MWContext *context)
{
  static Boolean guffaws_done = False;

  fe_InitIcons (context, MSG_BIFF_Unknown);

  if (!guffaws_done)
    {
      Widget widget = CONTEXT_DATA (context)->drawing_area;
      if (widget) {
	if (! XtIsRealized (widget)) abort ();
	guffaws_done = True;
	fe_globalData.fe_guffaw_scroll =
	  fe_WindowGravityWorks (CONTEXT_WIDGET(context), widget);
      }
    }
}

void
fe_DestroySaveToDiskContext(MWContext *context)
{
  fe_delete_cb (0, (XtPointer) context, 0);
}

void fe_cleanup_tooltips(MWContext *context);

void
fe_DestroyContext_part2(void *c)
{
	MWContext *context = (MWContext*)c;

  /* Destroy the compositor associated with the context. */
  if (context->compositor)
      {
          CL_DestroyCompositor(context->compositor);
          context->compositor = NULL;
      }

#ifdef MOZ_MAIL_NEWS
  MimeDestroyContextData(context);
#endif

  if (context->title) free (context->title);
  context->title = 0;

  /* If a synchronous url dialog is up, dont free the context. Once the
   * synchronous url dialog is over, we will free it.
   */
  if (fe_IsContextProtected(context)) {
    CONTEXT_DATA(context)->destroyed = 1;
  }
  else {
    free (CONTEXT_DATA (context));
    free (context);
  }
}

void
fe_DestroyContext (MWContext *context)
{
  PRBool observer_removed_p;    
  fe_ContextData* d = CONTEXT_DATA(context);
  Widget w = CONTEXT_WIDGET (context);
  struct fe_MWContext_cons *rest, *prev;

  /* Fix for bug #29631 */
  if (context == last_documented_xref_context)
    {
      last_documented_xref_context = 0;
      last_documented_xref = 0;
      last_documented_anchor_data = 0;
    }

  /* This is a hack. If the mailcompose window is going away and 
   * a tooltip was still there (or) was armed (a timer was set for it)
   * for a widget in the mailcompose window, then the destroying
   * mailcompose context would cause a core dump when the tooltip
   * timer hits. This could happen to a javascript window with toolbars
   * too if it is being closed on a timer.
   *
   * In this critical time of 3.x ship, we are fixing this by
   * always killing any tooltip that was present anywhere and 
   * remove the timer for any armed tooltop anywhere in the
   * navigator when any context is going away.
   *----
   * The proper thing to do would be
   *  - to check if the fe_tool_tips_widget is a child of the
   *    CONTEXT_WIDGET(context) and if it is, then cleanup the tooltip.
   */
  fe_cleanup_tooltips(context);
 

  if (context->is_grid_cell)
    CONTEXT_DATA (context)->being_destroyed = True;

  if (context->type == MWContextSaveToDisk) {
    /* We might be in an extend text selection on the text widgets.
       If we destroy this now, we will dump core. So before destroying
       we will disable extend of the selection. */
    XtCallActionProc(CONTEXT_DATA (context)->url_label, "extend-end",
				NULL, NULL, 0);
    XtCallActionProc(CONTEXT_DATA (context)->url_text, "extend-end",
				NULL, NULL, 0);
  }

#ifdef EDITOR
  if (context->type == MWContextEditor)
      fe_EditorCleanup(context);
#endif /*EDITOR*/

  if (context->is_grid_cell)
    w = d->main_pane;
  XP_InterruptContext (context);
  if (d->refresh_url_timer)
    XtRemoveTimeOut (d->refresh_url_timer);

  /* Progress takes a context, removing timeout */
  if (CONTEXT_DATA (context)->thermo_timer_id)
    {
		XtRemoveTimeOut (CONTEXT_DATA (context)->thermo_timer_id);
		
		CONTEXT_DATA (context)->thermo_timer_id = 0;
    }

  if (d->refresh_url_timer_url)
    free (d->refresh_url_timer_url);
  fe_DisposeColormap(context);

  /*
  ** We have to destroy the layout before calling XtUnmanageChild so that
  ** we have a chance to reparent the applet windows to a safe
  ** place. Otherwise they'll get destroyed.
  */
  fe_DestroyLayoutData (context);
  XtUnmanageChild (w);

  if (context->color_space) {
      IL_ReleaseColorSpace(context->color_space);
      context->color_space = NULL;
  }

  /* Destroy the image group context after removing the image group
     observer. */
  observer_removed_p =
      IL_RemoveGroupObserver(context->img_cx, fe_ImageGroupObserver,
                             (void *)context);
  IL_DestroyGroupContext(context->img_cx);
  context->img_cx = NULL;

  /* Fix for bug #29631 */
  if (context == last_documented_xref_context)
    {
      last_documented_xref_context = 0;
      last_documented_xref = 0;
      last_documented_anchor_data = 0;
    }

  fe_StopProgressGraph (context);
  fe_FindReset (context);
  SHIST_EndSession (context);
  fe_DisownSelection (context, 0, True);
  fe_DisownSelection (context, 0, False);

  if (! context->is_grid_cell) {
    if (context->type == MWContextSaveToDisk)
      XtRemoveCallback (w, XtNdestroyCallback, fe_AbortCallback, context);
    else
      XtRemoveCallback (w, XtNdestroyCallback, fe_delete_cb, context);

    XtRemoveEventHandler (w, StructureNotifyMask, False, fe_map_notify_eh,
			  context);

     if (context->type == MWContextDialog) {
      XtRemoveEventHandler (w, FocusChangeMask, False, fe_focus_notify_eh, 
			 context);
    }
  }
  XtDestroyWidget (w);

  if (CONTEXT_DATA (context)->ftd) free (CONTEXT_DATA (context)->ftd);
  if (CONTEXT_DATA (context)->sd) free (CONTEXT_DATA (context)->sd);
  if (CONTEXT_DATA(context)->find_data)
    XP_FREE(CONTEXT_DATA(context)->find_data);

  for (prev = 0, rest = fe_all_MWContexts;
       rest;
       prev = rest, rest = rest->next)
    if (rest->context == context)
      break;
  if (! rest) abort ();
  if (prev)
    prev->next = rest->next;
  else
    fe_all_MWContexts = rest->next;
  free (rest);


  /* Some window disappears. So we need to recreate windows menu of all
     contexts availables. Mark so. */
  for( rest=fe_all_MWContexts; rest; rest=rest->next )
    CONTEXT_DATA(rest->context)->windows_menu_up_to_date_p = False;

#ifdef JAVA
  LJ_DiscardEventsForContext(context);
#endif /* JAVA */

  XP_RemoveContextFromList(context);

  ET_RemoveWindowContext(context, fe_DestroyContext_part2, context);
}


/* This is called any time the user performs an action.
   It reorders the list of contexts so that the most recently
   used one is at the front.
 */
void
fe_UserActivity (MWContext *context)
{
  struct fe_MWContext_cons *rest, *prev;

  for (prev = 0, rest = fe_all_MWContexts;
       rest;
       prev = rest, rest = rest->next)
    if (rest->context == context)
      break;
  if (! rest) abort ();		/* not found?? */

  /* the new and the last are the same, we're done */
  if (context == fe_all_MWContexts->context)
    return;

  if (context->is_grid_cell) fe_SetGridFocus (context);

  if (! prev) return;		/* it was already first. */

  prev->next = rest->next;
  rest->next = fe_all_MWContexts;
  fe_all_MWContexts = rest;
}


void
fe_RefreshAllAnchors ()
{
  struct fe_MWContext_cons *rest;
  for (rest = fe_all_MWContexts; rest; rest = rest->next)
    LO_RefreshAnchors (rest->context);
}

/*
 * XXX Need to make all contexts be deleted through this. - dp
 */
void
fe_delete_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;

  if (fe_WindowCount == 1)
    {
      /* Unmap the window right away to give feedback that a delete
	 is in progress. */
      Widget widget = CONTEXT_WIDGET(context);
      Window window = (widget ? XtWindow(widget) : 0);
      if (window)
	XUnmapWindow (XtDisplay(widget), window);

      /* Now save files and exit. */
      fe_Exit (0);
    }
  else
    {
      if ( someGlobalContext == context )
      {
        fe_DestroyContext (context);
        fe_WindowCount--;
 
        someGlobalContext = XP_FindContextOfType(NULL, MWContextBrowser);
        if (!someGlobalContext)
              someGlobalContext = XP_FindContextOfType(NULL, MWContextMail);
        if (!someGlobalContext)
              someGlobalContext = XP_FindContextOfType(NULL, MWContextNews);
        if (!someGlobalContext)
              someGlobalContext = fe_all_MWContexts->context;

        XmAddWMProtocols(CONTEXT_WIDGET(someGlobalContext), 
				&WM_SAVE_YOURSELF,1);
        XmAddWMProtocolCallback(CONTEXT_WIDGET(someGlobalContext),
                WM_SAVE_YOURSELF,
                fe_wm_save_self_cb, someGlobalContext);
     }
     else
     {
        fe_DestroyContext (context);
        fe_WindowCount--;
 
     }

     if (fe_WindowCount <= 0)
	abort ();
    }
}

MWContext *
fe_WidgetToMWContext (Widget widget)
{
	struct fe_MWContext_cons* rest;
	struct fe_MWContext_cons* prev;
	MWContext* context;
	Widget transient_for;

	for (;;) {
		
		/*
		 *    Find the toplevel shell.
		 */
		while (widget != NULL && !XtIsWMShell(widget))
			widget = XtParent(widget);
		
		if (widget == NULL) /* doom */
			break;
		
		/*
		 *    Walk over the list of contexts. For each context,
		 *    get the shell, and compare with the widget's shell.
		 */
		rest = fe_all_MWContexts;
		prev = 0;
		for (; rest != NULL; prev = rest, rest = rest->next) {
			context = rest->context;
			if (context != NULL && CONTEXT_DATA(context) != NULL) {
				Widget mainw = CONTEXT_WIDGET(context);
				Widget parent;

				if (mainw == NULL) /* paranoia */
					continue;

				parent = XtParent(mainw);

				/*
				 *    The old version of this routine allowed
				 *    for both of these matches, so keep the semantics...
				 */
				if (mainw == widget || parent == widget) { /* match */
					/*
					 *    Push this context to top of list, so this
					 *    search is faster next time.
					 */
					if (fe_all_MWContexts != rest) {
						prev->next = rest->next;
						rest->next = fe_all_MWContexts;
						fe_all_MWContexts = rest;
					}
					return context;
				}
			}
		}

		/*
		 *    No match. Hmmmm, what if the shell is a transient,
		 *    let's try the shell it's transient for.
		 */
		if (XtIsSubclass(widget, transientShellWidgetClass)) {
			XtVaGetValues(widget, XmNtransientFor, &transient_for, 0);
			widget = transient_for;
		} else {
			break;
		}
	}
	
	/* There is no parent -- hope the caller can deal with a null */
	return 0;
}

/*
 * Now that we have grids, you can't just walk up to the parent shell
 * to find the context for a widget.  We are assuming here that the
 * motion event was always delivered to the drawingarea widget, I hope
 * that is correct --ejb
 */
MWContext *
fe_MotionWidgetToMWContext (Widget widget)
{
  struct fe_MWContext_cons *rest;
  for (rest = fe_all_MWContexts; rest; rest = rest->next) {
    if (CONTEXT_DATA (rest->context)->drawing_area == widget)
      return rest->context;
  }
  /* There is no parent -- hope the caller can deal with a null */
  return 0;
}


/* fe_MimimalNoUICleanup
 *
 * This does a cleanup of the only the absolute essential stuff.
 * - saves bookmarks, addressbook
 * - saves global history
 * - saves cookies
 * (since all these saves are protected by flags anyway, we wont endup saving
 *  again if all these happened before.)
 * - remove lock file if existent
 *
 * This will be called at the following points:
 * 1. when the x server dies [x_fatal_error_handler()]
 * 2. when window manages says 'SAVE_YOURSELF'
 * 3. at_exit_handler()
 */
void
fe_MinimalNoUICleanup()
{
  fe_SaveBookmarks ();

  PREF_SavePrefFile();
  NR_ShutdownRegistry();
#ifdef MOZ_SMARTUPDATE
  SU_Shutdown();
#endif

  RDF_Shutdown();
  GH_SaveGlobalHistory ();
  NET_SaveCookies(NULL);
  AltMailExit();
}

/* If there is whitespace at the beginning or end of string, removes it.
   The string is modified in place.
 */
char *
fe_StringTrim (char *string)
{
  char *orig = string;
  char *new;
  if (! string) return 0;
  new = string + strlen (string) - 1;
  while (new >= string && XP_IS_SPACE (new [0]))
    *new-- = 0;
  new = string;
  while (XP_IS_SPACE (*new))
    new++;
  if (new == string)
    return string;
  while (*new)
    *string++ = *new++;
  *string = 0;
  return orig;
}

/*
 * fe_StrEndsWith(char *s, char *endstr)
 *
 * returns TRUE if string 's' ends with string 'endstr'
 * else returns FALSE
 */
XP_Bool
fe_StrEndsWith(char *s, char *endstr)
{
  int l, lend;
  XP_Bool retval = FALSE;

  if (!endstr)
    /* All strings ends in NULL */
    return(TRUE);
  if (!s)
    /* NULL strings will never have endstr at its end */
    return(FALSE);

  lend = strlen(endstr);
  l = strlen(s);
  if (l >= lend && !strcmp(s+l-lend, endstr))
    retval = TRUE;
  return (retval);
}

char *
fe_Basename (const char *s)
{
    int len;
    char *p;

    if (!s) return (s);
    len = strlen(s);
    p = &s[len-1];
    
    while(--len > 0 && *p != '/') p--;
    if (*p == '/') p++;
    return (p);
}


/* Mail stuff */

const char *
FE_UsersMailAddress (void)
{
  static char *cached_uid = 0;
  char *uid, *name;
  if (fe_globalPrefs.email_address && *fe_globalPrefs.email_address)
    {
      if (cached_uid) free (cached_uid);
      cached_uid = 0;
      return fe_globalPrefs.email_address;
    }
  else if (cached_uid)
    {
      return cached_uid;
    }
  else
    {
      fe_DefaultUserInfo (&uid, &name, False);
      free (name);
      cached_uid = uid;
      return uid;
    }
}


const char *
FE_UsersRealMailAddress (void)
{
  static char *cached_uid = 0;
  if (cached_uid)
    {
      return cached_uid;
    }
  else
    {
      char *uid, *name;
      fe_DefaultUserInfo (&uid, &name, True);
      free (name);
      cached_uid = uid;
      return uid;
    }
}


const char *
FE_UsersFullName (void)
{
  static char *cached_name = 0;
  char *uid, *name;
  if (fe_globalPrefs.real_name && *fe_globalPrefs.real_name)
    {
      if (cached_name) free (cached_name);
      cached_name = 0;
      return fe_globalPrefs.real_name;
    }
  else if (cached_name)
    {
      return cached_name;
    }
  else
    {
      fe_DefaultUserInfo (&uid, &name, False);
      free (uid);
      cached_name = name;
      return name;
    }
}


const char *
FE_UsersOrganization (void)
{
  static char *cached_org = 0;
  if (cached_org)
    free (cached_org);
  cached_org = strdup (fe_globalPrefs.organization
		       ? fe_globalPrefs.organization
		       : "");
  return cached_org;
}

#ifdef MOZ_MAIL_NEWS
const char *
FE_UsersSignature (void)
{
  static char *signature = NULL;
  XP_File file;
  time_t sig_date = 0;
	
  if (signature)
    XP_FREE (signature);
		
  file = XP_FileOpen (fe_globalPrefs.signature_file,
		      xpSignature, XP_FILE_READ);
	
  if (file)
    {
      struct stat st;
      char buf [1024];
      char *s = buf;
	
      int left = sizeof (buf) - 2;
      int size;
      *s = 0;

      if (!fstat (fileno (file), &st))
	sig_date = st.st_mtime;

      while ((size = XP_FileRead (s, left, file)) && left > 0)
	{
	  left -= size;
	  s += size;
	}

      *s = 0;

      /* take off all trailing whitespace */
      s--;
      while (s >= buf && isspace (*s))
	*s-- = 0;
      /* terminate with a single newline. */
      s++;
      *s++ = '\n';
      *s++ = 0;
      XP_FileClose (file);
      if ( !strcmp (buf, "\n"))
	signature = NULL;
      else
	signature = strdup (buf);
    }
  else
    signature = NULL;

  /* The signature file date has changed - check the contents of the file
     again, and save that date to the preferences file so that it is checked
     only when the file changes, even if Netscape has been restarted in the
     meantime. */
  if (fe_globalPrefs.signature_date != sig_date)
    {
      MWContext *context =
	XP_FindContextOfType(0, MWContextMessageComposition);
      if (!context) context = fe_all_MWContexts->context;
      MISC_ValidateSignature (context, signature);
      fe_globalPrefs.signature_date = sig_date;

      if (!XFE_SavePrefs ((char *) fe_globalData.user_prefs_file,
			  &fe_globalPrefs))
	fe_perror (context, XP_GetString( XFE_ERROR_SAVING_OPTIONS ) );
    }

  return signature;
}
#endif  /* MOZ_MAIL_NEWS */


int32
FE_GetContextID (MWContext * window_id)
{
   return((int32) window_id);
}

XP_Bool
XFE_UseFancyFTP (MWContext *context)
{
  return CONTEXT_DATA (context)->fancy_ftp_p;
}

XP_Bool
XFE_UseFancyNewsgroupListing (MWContext * window_id)
{
  return False;
}

/* FE_ShowAllNewsArticles
 *
 * Return true if the user wants to see all newsgroup  
 * articles and not have the number restricted by
 * .newsrc entries
 */
XP_Bool
XFE_ShowAllNewsArticles (MWContext *window_id)
{
    return(FALSE);  /* temporary LJM */
}

int 
XFE_FileSortMethod (MWContext * window_id)
{
   return (SORT_BY_NAME);
}

int16
INTL_DefaultDocCharSetID(MWContext *cxt)
{
	int16	csid;

	if (cxt)
	{
		INTL_CharSetInfo csi = LO_GetDocumentCharacterSetInfo(cxt);
		if (INTL_GetCSIDocCSID(csi))      
		{
			csid = INTL_GetCSIDocCSID(csi);
		}
		else if (cxt->fe.data && cxt->fe.data->xfe_doc_csid)
		{
			csid = cxt->fe.data->xfe_doc_csid;
		}
		else
		{
			csid = fe_globalPrefs.doc_csid;
		}
	}
	else
	{
		csid = fe_globalPrefs.doc_csid;
	}

	return csid;
}

char *
INTL_ResourceCharSet(void)
{
	return fe_LocaleCharSetName;
}

void
INTL_Relayout(MWContext *pContext)
{
  if(XP_IsContextBusy(pContext) == FALSE)
	{
	  fe_ReLayout(pContext, NET_DONT_RELOAD);
	}
}



typedef struct fe_timeout {
  TimeoutCallbackFunction func;
  void* closure;
  XtIntervalId timer;
  uint32 serial_num;
  struct fe_timeout *next;
} fe_timeout;

static uint32 fe_timeout_serial_num = 0; /* Unique token for each timeout */
static fe_timeout *fe_TimeoutList = NULL;

static Bool
remove_timeout_from_list(uint32 serial_num, Bool clear_timeout)
{
  fe_timeout **p, *t;

  p = &fe_TimeoutList;
  while ((t = *p)) {
      if (t->serial_num == serial_num) {
          *p = t->next;
          if (clear_timeout)
              XtRemoveTimeOut(t->timer);
          XP_FREE(t);
          return TRUE;
      }
      p = &t->next;
  }
  return FALSE;
}

static void
fe_do_timeout(XtPointer p, XtIntervalId* id)
{
  fe_timeout* timer = (fe_timeout*) p;
  XP_ASSERT(timer->timer == *id);
  (*timer->func)(timer->closure);
  if (!remove_timeout_from_list(timer->serial_num, FALSE)) {
      XP_ASSERT(0);
  }
}

void* 
FE_SetTimeout(TimeoutCallbackFunction func, void* closure, uint32 msecs)
{
  fe_timeout* timer;
  timer = XP_NEW(fe_timeout);
  if (!timer) return NULL;
  timer->func = func;
  timer->closure = closure;
  timer->timer = XtAppAddTimeOut(fe_XtAppContext, msecs, fe_do_timeout, timer);
  timer->serial_num = ++fe_timeout_serial_num;
  timer->next = fe_TimeoutList;
  fe_TimeoutList = timer;
  return (void*)fe_timeout_serial_num;
}

void 
FE_ClearTimeout(void* timer_id)
{
  remove_timeout_from_list((uint32)timer_id, TRUE);
}

char *
FE_GetCipherPrefs(void)
{
    if (fe_globalPrefs.cipher == NULL)
	return NULL;
    return(strdup(fe_globalPrefs.cipher));
}

void
FE_SetCipherPrefs(MWContext *context, char *cipher)
{
  if (fe_globalPrefs.cipher) {
    if (!strcmp(fe_globalPrefs.cipher, cipher))
      return;
    XP_FREE(fe_globalPrefs.cipher);
  }
  fe_globalPrefs.cipher = strdup(cipher);
  if (!XFE_SavePrefs ((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
  {
      if (context == NULL) {
	  MWContext *someContext;
	  /* Type to find a context */
	  someContext = XP_FindContextOfType(NULL, MWContextBrowser);
	  if (!someContext)
	      someContext = XP_FindContextOfType(NULL, MWContextMail);
	  if (!someContext)
	      someContext = XP_FindContextOfType(NULL, MWContextNews);
	  if (!someContext)
	      someContext = fe_all_MWContexts->context;
	  context = someContext;
      }
      if (context != NULL)
	  fe_perror (context, XP_GetString( XFE_ERROR_SAVING_OPTIONS));
  }
}


/************************
 * File status routines *
 ************************/
/*
 * File changes since last seen.
 * For error case of file not present, it returns FALSE and doesn't change
 *	return value new_mtime.
 * If file has changed, returns TRUE and new_mtime if not null is updated
 *	to the new modified time.
 * If file has not changed, return FALSE and doesn't change new_mtime.
 */
XP_Bool
fe_isFileChanged(char *name, time_t mtime, time_t *new_mtime)
{
    XP_StatStruct st;
    XP_Bool ret = FALSE;
    
    if (name && *name && !stat(name, &st))
      if (st.st_mtime != mtime) ret = TRUE;

    if (ret && new_mtime)
      *new_mtime = st.st_mtime;

    return (ret);
}

/*
 * File exists
 */
Boolean
fe_isFileExist(char *name)
{
    XP_StatStruct st;
    if (!name || !*name) return (False);
    if (!stat (name, &st))
	return (True);
    else
	return (False);
}

/*
 * File exists and is readable.
 */
Boolean
fe_isFileReadable(char *name)
{
    FILE *fp;
    if (!name || !*name) return (False);
    fp = fopen(name, "r");
    if (fp) {
	fclose(fp);
	return (True);
    }
    else
	return (False);
}

/*
 * File is a directory
 */
Boolean
fe_isDir(char *name)
{
    XP_StatStruct st;
    if (!name || !*name) return (False);
    if (!stat (name, &st) && S_ISDIR(st.st_mode))
	return (True);
    else
	return (False);
}


/*
 * Layering support.  LO_RefreshArea is called through compositor.
 */

void
fe_RefreshArea(MWContext *context, int32 x, int32 y,
                uint32 width, uint32 height)
{
    if(context->compositor)
        {
            XP_Rect rect;
            
            rect.left = x;
            rect.top = y;
            rect.right = x + width;
            rect.bottom = y + height;
            CL_UpdateDocumentRect((context)->compositor, &rect, PR_TRUE);
        }
#ifdef EDITOR
    if (EDT_IS_EDITOR(context))
        fe_EditorRefreshArea(context, x, y, width, height);
#endif /*EDITOR*/
}

void
fe_RefreshAreaRequest(MWContext *context, int32 x, int32 y,
					  uint32 width, uint32 height)
{
    if(context->compositor) {
		XP_Rect rect;
            
		rect.left = x;
		rect.top = y;
		rect.right = x + width;
		rect.bottom = y + height;
		CL_UpdateDocumentRect((context)->compositor, &rect, PR_FALSE);
	}
}

extern void fe_HTMLViewDoPopup (MWContext *context, CL_Layer *layer,
								CL_Event *layer_event);
extern void fe_HTMLDragSetLayer(CL_Layer *layer);

/* Handle events on a layer-specific basis. */
PRBool FE_HandleLayerEvent(MWContext *context, CL_Layer *layer,
                           CL_Event *layer_event)
{
    PRBool handled_event_p = TRUE;
    Boolean synthesized_event_p = FALSE;
    fe_EventStruct *fe_event = (fe_EventStruct *)layer_event->fe_event;
    XEvent *event = NULL;
    fe_MouseActionEnum mouse_action = FE_INVALID_MOUSE_ACTION;

    if (fe_event)
        {
            mouse_action = fe_event->mouse_action;
        }
    else
        {
            /* This means that we have a synthesized event, so fill
               in the FE part. */

			int32 layer_x_offset, layer_y_offset;

            synthesized_event_p = TRUE;
            fe_event = XP_NEW_ZAP(fe_EventStruct);
            XP_ASSERT(fe_event);

            /* Create an XEvent. */
            event = XP_NEW_ZAP(XEvent);
            XP_ASSERT(event);

			layer_x_offset = CL_GetLayerXOffset(layer);
			layer_y_offset = CL_GetLayerYOffset(layer);

            /* XXX This part of the event synthesis code is currently based
               on the mouse bindings specified in the default resource file.
               We need to eventually change it so that it will work with any
               set of mouse bindings that the user specifies. */

            switch (layer_event->type)
                {
                case CL_EVENT_MOUSE_BUTTON_DOWN:
                    event->type = ButtonPress;
                    event->xbutton.x = layer_event->x + layer_x_offset -
                        CONTEXT_DATA (context)->document_x;
                    event->xbutton.y = layer_event->y + layer_y_offset -
                        CONTEXT_DATA (context)->document_y;
                    event->xbutton.time =
                        XtLastTimestampProcessed(XtDisplay(CONTEXT_WIDGET(context)));
                    
                    switch (layer_event->which)
                        {
                        case 1: /* Left mouse. */
                            event->xbutton.button = 1;
                            mouse_action = FE_ARM_LINK;
                            break;

                        case 2: /* Right mouse. */
                            event->xbutton.button = 3;
                            mouse_action = FE_POPUP_MENU;
                            break;

                        default:
                            XP_ASSERT(0);
                            break;
                        }
                    break;

                case CL_EVENT_MOUSE_BUTTON_UP:
                    event->type = ButtonRelease;
                    event->xbutton.x = layer_event->x + layer_x_offset -
                        CONTEXT_DATA (context)->document_x;
                    event->xbutton.y = layer_event->y + layer_y_offset -
                        CONTEXT_DATA (context)->document_y;
                    event->xbutton.time =
                        XtLastTimestampProcessed(XtDisplay(CONTEXT_WIDGET(context)));
                    
                    switch (layer_event->which)
                        {
                        case 1: /* Left mouse. */
                            event->xbutton.button = 1;
                            mouse_action = FE_ACTIVATE_LINK;
                            break;

                        default:
                            XP_ASSERT(0);
                            break;
                        }
                    break;
                    
                case CL_EVENT_MOUSE_MOVE:
                    event->type = MotionNotify;
                    event->xmotion.x = layer_event->x + layer_x_offset -
                        CONTEXT_DATA (context)->document_x;
                    event->xmotion.y = layer_event->y + layer_y_offset -
                        CONTEXT_DATA (context)->document_y;
                    event->xmotion.time =
                        XtLastTimestampProcessed(XtDisplay(CONTEXT_WIDGET(context)));
                    
                    switch (layer_event->which)
                        {
                        case 0: /* No button. */
                            mouse_action = FE_DESCRIBE_LINK; 
                            break;
                            
                        case 1: /* Left mouse. */
                            event->xmotion.state = Button1Mask;
                            mouse_action = FE_DISARM_LINK_IF_MOVED;
                            break;

                        case 2: /* Right mouse. */
                            event->xmotion.state = Button3Mask;
                            mouse_action = FE_DISARM_LINK_IF_MOVED;
                            break;

                        default:
                            XP_ASSERT(0);
                            break;
                        }
                    break;

		case CL_EVENT_MOUSE_ENTER:
		case CL_EVENT_MOUSE_LEAVE:
		case CL_EVENT_KEY_FOCUS_GAINED:
		case CL_EVENT_KEY_FOCUS_LOST:
		    if (synthesized_event_p) {
		        XP_FREE(event);
			XP_FREE(fe_event);
		    }
		  return FALSE;

                default:
                    XP_ASSERT(0);
                    break;
                }

#ifdef LAYERS_FULL_FE_EVENT
            fe_event->event = event;
            fe_event->av = NULL;
            fe_event->ac = NULL;
            fe_event->mouse_action = mouse_action;
#else
	    fe_event_stuff(context,fe_event,event,0,0,mouse_action);
#endif
            
            layer_event->fe_event = fe_event;

        }
    
    switch (mouse_action)
        {
            /* These correspond to 
               layer_event->type = CL_EVENT_MOUSE_BUTTON_DOWN */

        case FE_ARM_LINK:
            /* hack to allow Motif drag and drop to catch Layer event. */
            fe_HTMLDragSetLayer(layer);
            
            fe_arm_link_action_for_layer(context, layer, layer_event);
            break;
                    
        case FE_EXTEND_SELECTION:
            fe_extend_selection_action_for_layer(context, layer,
                                                 layer_event);
            break;

        case FE_POPUP_MENU:
	    fe_HTMLViewDoPopup (context, layer, layer_event);
            break;

            /* These correspond to
               layer_event->type = CL_EVENT_MOUSE_BUTTON_UP */

        case FE_ACTIVATE_LINK:
            fe_activate_link_action_for_layer(context, layer,
                                              layer_event);
            break;

        case FE_DISARM_LINK:
#ifdef LAYERS_SEPARATE_DISARM
            fe_disarm_link_action_for_layer(context, layer,
                                            layer_event);
#else
#ifdef DEBUG
	    printf("FE_HandleLayerEvent(): unexpected FE_DISARM_LINK\n");
#endif
#endif
            break;

            /* These correspond to
               layer_event->type = CL_EVENT_MOUSE_MOVE */

        case FE_DESCRIBE_LINK:
            fe_describe_link_action_for_layer(context, layer,
                                              layer_event);
            break;
                    
        case FE_DISARM_LINK_IF_MOVED:
            fe_disarm_link_if_moved_action_for_layer(context, layer,
                                                     layer_event);
            break;
            
            /* No actions corresponding to
               layer_event->type = CL_EVENT_MOUSE_BUTTON_MULTI_CLICK */

	  case FE_KEY_UP:
	    fe_key_up_in_text_action_for_layer(context, layer,
					       layer_event);
	    break;
            
	  case FE_KEY_DOWN:
	    fe_key_down_in_text_action_for_layer(context, layer,
						 layer_event);
	    break;
            
        default:
            XP_ASSERT(0);
            break;
        }

    if (synthesized_event_p)
        {
            if (mouse_action == FE_ACTIVATE_LINK)
	      {
		/* disarm is now triggered by activate, instead of
		 *  by the mouseclick, so calling 
		 *  fe_activate_link_action_for_layer() above triggered
		 *  the disarm already. -- francis
		 */
#ifdef LAYERS_SEPARATE_DISARM
		fe_disarm_link_action_for_layer(context, layer,
						layer_event);
#endif
	      }
            XP_FREE(event);
            XP_FREE(fe_event);
        }

    return handled_event_p;
}

/* XXX - For now, unix does not have windowless plugins */
PRBool FE_HandleEmbedEvent(MWContext *context, LO_EmbedStruct *embed,
                           CL_Event *event)
{
  return PR_FALSE;
}


/* the purpose of the following code is to provide the backend
 * a way to get temp files associated with each ldap server.
 * these files should have unique names, meaning for each different
 * ldap server, a different file is used. at the same time, if the
 * backend queries with the same ldap server, the same file name should
 * be returned (because we want to allow user to continue from previous
 * searches) 
 * 
 * anything with ifdef _XP_TMP_FILENAME_FOR_LDAP_ in it needs to be
 * implemented. right now we are just returning hardcoded names to test
 * the ldap features.
 */

/*
#define _XP_TMP_FILENAME_FOR_LDAP_
*/

char* fe_GetLDAPTmpFile(char *name) {
	char* home = getenv("HOME");
	static char tmp[1024];
	if (!home) home = "";
	if (!name) return NULL;

	sprintf(tmp, "%.900s/.netscape/", home);

#ifdef _XP_TMP_FILENAME_FOR_LDAP_
	/* we need to write this */
	/* what we need: look for the temp name associated with the
	 * ldap server specified in the array "name".
	 * if we find it, we return it, if not, we create 
	 * a new tmp file name and "remember" that it is associated
	 * with this particular ldap server - benjie */

	/* here we look for it */

	if (HA_I_FOUND_IT) {
		return THE_FILE_NAME;
	} else {
		/* we create a new one */
		char *ldapfile=NULL;
		strcat(tmp,"ldapXXXXXX");
		ldapfile = mktemp(tmp);
		if (!ldapfile) return null;
		else {
			PR_snprintf(tmp, sizeof (tmp),"%s.nab",ldapfile);
			/ok, now we save the temp file name*/

			/* saving temp file name associated with the ldap server here */

			return tmp;
		}
 	}
	return NULL;
#else
	if (strcmp(name,"scrappy")==0) {
		strcat(tmp,"nsldap.nab\0");
		return tmp;
	} else if (strcmp(name,"umich")==0) {
		strcat(tmp,"umich.nab\0");
		return tmp;
	} else return NULL;
#endif
}


/* this should be called from the backend (XP_FileName) */
/* for now it is only be called from fe */
char* FE_GetFileName(char *name, XP_FileType type) {
	switch(type) {
		case xpAddrBook:
			return fe_GetLDAPTmpFile(name);
		default:
			return NULL;
	}
}

/*
 *    Walks over a tree, calling mappee callback on each widget.
 */
XtPointer
fe_WidgetTreeWalk(Widget widget, fe_WidgetTreeWalkMappee callback,
		  XtPointer data)
{
  Arg       av[8];
  Cardinal  ac;
  Widget*   children;
  Cardinal  nchildren;
  Cardinal  i;
  XtPointer rv;

  if (widget == NULL || callback == NULL)
      return 0;

  if (XtIsSubclass(widget, compositeWidgetClass)) {

      ac = 0;
      XtSetArg(av[ac], XmNchildren, &children); ac++;
      XtSetArg(av[ac], XmNnumChildren, &nchildren); ac++;
      XtGetValues(widget, av, ac);
      
      for (i = 0; i < nchildren; i++) {
	  rv = fe_WidgetTreeWalk(children[i], callback, data);
	  if (rv != 0)
	      return rv;
      }
  }

  return (callback)(widget, data);
}

/*
 * fe_WidgetTreeWalkChildren
 *
 * Intension here is to call the mappee callback for all children in the
 * tree taking into account the cascade menus.
 */
XtPointer
fe_WidgetTreeWalkChildren(Widget widget, fe_WidgetTreeWalkMappee callback,
			  XtPointer closure)
{
  Widget *buttons = 0, menu = 0;
  Cardinal nbuttons = 0;
  int i;
  XtPointer ret = 0;
  XtVaGetValues (widget, XmNchildren, &buttons, XmNnumChildren, &nbuttons, 0);
  for (i = 0; ret == 0 && i < nbuttons; i++)
    {
      Widget item = buttons[i];
      if (XmIsToggleButton(item) || XmIsToggleButtonGadget(item) ||
	      XmIsPushButton(item) || XmIsPushButtonGadget(item) ||
	      XmIsCascadeButton(item) || XmIsCascadeButtonGadget(item))
	ret = (callback) (item, closure);
      if (ret != 0) break;
      if (XmIsCascadeButton(item) || XmIsCascadeButtonGadget(item)) {
	XtVaGetValues (item, XmNsubMenuId, &menu, 0);
	if (menu)
	  ret = fe_WidgetTreeWalkChildren(menu, callback, closure);
      }
    }
  return(ret);
}


static XtPointer
fe_find_widget_mappee(Widget widget, XtPointer data)
{
    char* match_name = (char*)data;
    char* name = XtName(widget);

    if (strcmp(name, match_name) == 0) {
	return (XtPointer) widget; /* non-zero, will force termination of walk */
    } else {
	return 0;
    }
}

Widget
fe_FindWidget(Widget top, char* name)
{
    XtPointer rv;

    rv = fe_WidgetTreeWalk(top, fe_find_widget_mappee, (XtPointer)name);
    
    return (Widget)rv;
}

/*
 *    Tool tips.
 */
static Widget       fe_tool_tips_widget;   /* current hot widget */
static XtIntervalId fe_tool_tips_timer_id; /* timer id for non-movement */
static Widget       fe_tool_tips_shell;    /* posted tips shell */

/*
 *    New tool tip code.
 */
typedef struct TipInfo {
	struct TipInfo* m_next;
	XtPointer       m_key;
	union {
		/* if callback == GADGET_DUMMY, gadgets is valid */
		XtCallbackRec   callback_rec;
		struct TipInfo* gadgets;
	} u;
	int     x, y; /* mouse pos */
	XP_Bool rePosition; 
} TipInfo;

#define m_gadgets  u.gadgets
#define m_callback u.callback_rec.callback
#define m_closure  u.callback_rec.closure

static TipInfo*
TipInfoNew(XtPointer key, XtCallbackProc callback, XtPointer closure)
{
	TipInfo* info;

	info = XP_NEW(TipInfo);

	info->m_key = key;
	info->m_callback = callback;
	info->m_closure = closure;
	info->m_next = NULL;
	info->x = -10;
	info->y = -10;
	info->rePosition = False;
	return info;
}

static void
TipInfoDelete(TipInfo* info)
{
	XP_FREE(info);
}

static TipInfo*
TipInfoListInsert(TipInfo** list, TipInfo* info)
{
	info->m_next = *list;
	*list = info;

	return info;
}

static void
TipInfoListDelete(TipInfo* info)
{
	TipInfo* next;
	while (info != NULL) {
		next = info->m_next;
		XP_FREE(info);
		info = next;
	}
}

static TipInfo*
TipInfoListFind(TipInfo* head, XtPointer key)
{
	TipInfo* foo;

	for (foo = head; foo != NULL; foo = foo->m_next) {
		if (foo->m_key == key)
			break;
	}

	return foo;
}

static TipInfo*
TipInfoListRemove(TipInfo** list, XtPointer key)
{
	TipInfo* info = *list;
	TipInfo* prev = NULL;

	for (; info != NULL; prev = info, info = info->m_next) {
		if (info->m_key == key) {
			if (prev != NULL) {
				prev->m_next = info->m_next;
			} else {
				*list = info->m_next;
			}
			TipInfoDelete(info);
			break;
		}
	}

	return *list;
}

static TipInfo* tips_manager_list;
#ifdef USE_TIP_WIDGET_LIST
static TipInfo* tips_widget_list;
#endif

static void
tip_dummy_manager_cb(Widget widget, XtPointer closure, XtPointer cb) {}

#define TIPINFO_IS_MANAGER(i) ((i)->m_callback == tip_dummy_manager_cb)

/*
 *    When there is no callback info, there is no need to allocate
 *    memory to save it. Use this as a dummy callback info.
 */
static TipInfo tips_no_callback_info; /* must be 0s */

static void
tips_widget_death_cb(Widget widget, XtPointer closure, XtPointer cb)
{
    TipInfo* info = (TipInfo*)closure;

	XtRemoveCallback(widget, XmNdestroyCallback, tips_widget_death_cb, info);
	if (TIPINFO_IS_MANAGER(info)) {
		TipInfoListDelete(info->m_gadgets);
		TipInfoListRemove(&tips_manager_list, (XtPointer)info);
	}
#ifdef USE_TIP_WIDGET_LIST
	else
	{
		TipInfoListRemove(&tips_widget_list, info);
	}
#endif
	if (info != &tips_no_callback_info)
		TipInfoDelete(info);

	if (fe_tool_tips_widget ==  widget)
		fe_tool_tips_widget = NULL;
}

static TipInfo*
TipInfoNewManager(Widget manager)
{
	TipInfo* info =  TipInfoNew((XtPointer)manager, tip_dummy_manager_cb, NULL);

	XtAddCallback(manager, XmNdestroyCallback, tips_widget_death_cb, info);

	return info;
}

static TipInfo*
TipInfoNewWidget(Widget widget, XtCallbackProc callback, XtPointer closure)
{
	TipInfo* info;

	if (callback != NULL) {
		info = TipInfoNew((XtPointer)widget, callback, closure);
#ifdef USE_TIP_WIDGET_LIST
		TipInfoListInsert(&tips_widget_list, info);
#endif
		XtAddCallback(widget, XmNdestroyCallback, tips_widget_death_cb, info);
	} else {
		info = &tips_no_callback_info;
	}
	return info;
}

void
fe_cleanup_tooltips(MWContext *context)
{
    fe_tool_tips_widget = NULL;

    /*
     *    Stage two? Any event should zap that.
     */
    if (fe_tool_tips_shell) {
	XtDestroyWidget(fe_tool_tips_shell);
	fe_tool_tips_shell = NULL;

	/* Mark the tooltips not showing */
	fe_tooltip_is_showing = False;
    }
    
    /*
     *   Stage one?
     */
    if (fe_tool_tips_timer_id) {
	XtRemoveTimeOut(fe_tool_tips_timer_id);
	fe_tool_tips_timer_id = 0;
    }
}

static XP_Bool
fe_display_docString(MWContext* context,
					 Widget widget, TipInfo* info, XEvent* event,
					 Boolean erase)
{
    char *s;

    if (!erase) {

		s = NULL;

		/*
		 *    Do callback so user can change the string.
		 */
		if (info->m_callback != NULL) {
			XFE_TipStringCallbackStruct cb_info;
			
			cb_info.reason = XFE_DOCSTRING;
			cb_info.event = event;
			cb_info.string = &s;
			/* Tao
			 */
			cb_info.x = info->x;
			cb_info.y = info->y;

			(*info->m_callback)(widget, info->m_closure, &cb_info);
		}

		if (s == NULL) 
		{
			s = XfeSubResourceGetWidgetStringValue(widget,
												   "documentationString",
												   "DocumentationString");
		}

	} else { /* erasing */
		s = "";
	}
	
#ifdef DEBUG
    if (s == NULL) {
        static char buf[128];
		s = buf;    
		sprintf(s, "Debug: no documentationString resource for widget %s",
				XtName(widget));
    }
#endif /*DEBUG*/

    if (context == NULL || s == NULL)
		return False;

	XFE_Progress(context, s);

	return True;
}

static void
fe_tooltips_display_stage_one(Widget widget, XEvent* event,
							  TipInfo* info, Boolean begin)
{
    MWContext* context = fe_WidgetToMWContext(widget);
    if (context)
        fe_display_docString(context, widget, info, event, !begin);
}

static Widget
fe_tooltip_create_effects(Widget parent, char* name, char* string)
{
    Widget shell;
    Widget label;
    Visual *v = 0;
    Colormap cmap = 0;
    Cardinal depth = 0;
    XmFontList fontList;
    XmString xm_string;

    shell = parent;
    while (XtParent(shell) && !XtIsShell(shell)) {
	shell = XtParent(shell);
    }

    if (shell == NULL || XtParent(shell) == NULL)
        return NULL;

    XtVaGetValues(shell, XtNvisual, &v, XtNcolormap, &cmap,
		   XtNdepth, &depth, 0);

    XtVaGetValues(parent, XmNfontList, &fontList, NULL);

    shell = XtVaCreateWidget(name,
			     overrideShellWidgetClass,
			     XtParent(shell), /* the app */
			     XmNvisual, v,
			     XmNcolormap, cmap,
			     XmNdepth, depth,
			     XmNborderWidth, 1,
			     NULL);

    xm_string = XmStringCreateLocalized(string);

    label = XtVaCreateManagedWidget("tipLabel",
				    xmLabelWidgetClass,
				    shell,
				    XmNlabelType, XmSTRING,
				    XmNlabelString, xm_string,
				    NULL);
    XmStringFree(xm_string);
    XtManageChild(label);

    return label;
}

static Widget
fe_tooltips_display_stage_two(Widget widget, TipInfo* info)
{
    Widget parent;
    Widget label;
    Dimension width;
    Dimension border_width;
    Dimension height;
    Position x_root;
    Position y_root;
    Position y_root_orig;
    Screen* screen;
    Position x_info = 0;
    Position y_info = 0;

    char* s = XfeSubResourceGetWidgetStringValue(widget, 
												 "tipString", 
												 "TipString");

	/*
	 *    Do callback so user can change the string.
	 */
	if (info->m_callback != NULL) {
		XFE_TipStringCallbackStruct cb_info;
		XAnyEvent any_event;

		any_event.type = -1;
		any_event.serial = 0;
		any_event.send_event = False;
		any_event.display = XtDisplay(widget);
		any_event.window = XtWindow(widget);

		cb_info.reason = XFE_TIPSTRING;
		cb_info.event = (XEvent*)&any_event;
		cb_info.string = &s;

		/* Tao
		 */
		cb_info.x = info->x;
		cb_info.y = info->y;

		(*info->m_callback)(widget, info->m_closure, &cb_info);

		/* Tao
		 */
		x_info = cb_info.x;
		y_info = cb_info.y;
	}

#ifdef DEBUG
    if (s == NULL && 
		!(info->rePosition &&
		  x_info >= 0 && 
		  y_info >= 0)) {/* prompt debug tooltip iff it is not a grid/html view
							   */
        static char buf[256];
		s = buf;
		sprintf(s, "Debug: no tipString resource for widget %s\n"
				"This message only appears in a DEBUG build",
				XtName(widget));
    }
#endif /*DEBUG*/

    if (s == NULL || !XP_STRLEN(s))
      return NULL;

    parent = XtParent(widget);

    label = fe_tooltip_create_effects(parent, "tipShell", s);
    if (label == NULL)
        return NULL;

    parent = XtParent(label);

	/* Tao: compute x, y only when x_info && y_info is not set
	 */
        /* francis: yes, but do that before making sure it fits on the screen */

    XtVaGetValues(widget, XmNwidth, &width, XmNheight, &height, 0);
    XtTranslateCoords(widget, 0, 0, &x_root, &y_root_orig);
    x_root += (width/2); /* positon in center of button */
    y_root = y_root_orig + height + 5;

    /* moved here by francis; formerly right before XtVaSetValues(), below */
	/* Tao
	 */
	if (info->rePosition == True &&
		x_info >= 0 && 
		y_info >= 0) {
		XtTranslateCoords(widget, x_info, y_info, &x_root, &y_root);
	}/* if */

    /*
     *    Make sure it fits on screen.
     */
    XtVaGetValues(parent, XmNborderWidth, &border_width, 0);
    XtVaGetValues(label, XmNwidth, &width, XmNheight, &height, 0);
    screen = XtScreen(label);

    height += (2*border_width);
    width += (2*border_width);

    if (x_root + width > WidthOfScreen(screen))
	x_root = WidthOfScreen(screen) - width;
    else if (x_root < 0)
	x_root = 0;
    if (y_root + height > HeightOfScreen(screen))
	y_root = y_root_orig - height - 5;
    else if (y_root < 0)
	y_root = 0;

	XtVaSetValues(parent, XmNx, x_root, XmNy, y_root, 0);

    /*
     *    Make sure the user cannot shoot themselves with a random
     *    geometry spec. No more attack of the killer tomatoes...djw
     */
    {
		char buf[128];
		sprintf(buf, "%dx%d", width, height);
		XtVaSetValues(parent, XmNwidth, width, XmNheight, height,
					  XmNgeometry, buf, 0);
    }

	/* Mark the tooltips showing */
	fe_tooltip_is_showing = True;

    XtPopup(parent, XtGrabNone);

    return parent;
}

static void
fe_tooltips_stage_two_timeout(XtPointer closure, XtIntervalId *id)
{
    TipInfo*   info = (TipInfo*)closure;
	Widget     widget = fe_tool_tips_widget;
    Widget     shell;

    /* Clear the timer id that we store in the context as once the timeout
     * has triggered (that is why we are here), the timeout is automatically
     * removed. Else our event handler will go and remove the timeout again.
     */
    fe_tool_tips_timer_id = 0;

    if (fe_tool_tips_shell == NULL && widget != NULL) {
		shell = fe_tooltips_display_stage_two(widget, info);
		fe_tool_tips_shell = shell;
    }

}

static void
tip_dummy_demo_cb(Widget widget, XtPointer c, XtPointer d) {}

typedef struct {
	MWContext *context;
#if DO_NOT_PASS_LAYER_N_EVENT
	char      *alt_text;
	int       x;
	int       y;
#else
	CL_Layer  *layer;
	CL_Event  *layer_event;
#endif
} HTMLTipData_t;

static Widget
fe_HTMLTips_display_stage_two(Widget widget, HTMLTipData_t* info)
{
    Widget parent;
    Widget label;
    Dimension width;
    Dimension border_width;
    Dimension height;
    Position x_root;
    Position y_root;
    Position y_root_orig;
    Screen* screen;
    Position x_info;
    Position y_info;
    char* s = NULL;

	if (!info)
		return NULL;


	/*
	 *    Do callback so user can change the string.
	 */
	{
		XFE_TipStringCallbackStruct cb_info;
		cb_info.reason = XFE_TIPSTRING;
		cb_info.event = (XEvent*) NULL;
		cb_info.string = &s;

		cb_info.x = -10;
		cb_info.y = -10;
#if DO_NOT_PASS_LAYER_N_EVENT
		fe_HTMLViewTooltips(info->context, info->x, info->y, info->alt_text,
							&cb_info);
#else
		fe_HTMLViewTooltips(info->context, info->layer, info->layer_event, 
							&cb_info);
#endif
		XP_FREEIF(info);

		x_info = cb_info.x;
		y_info = cb_info.y;
	}
    if (s == NULL || !XP_STRLEN(s))
      return NULL;

    parent = XtParent(widget);

    label = fe_tooltip_create_effects(parent, "tipShell", s);
    if (label == NULL)
        return NULL;

    parent = XtParent(label);

	/* Tao: compute x, y only when x_info && y_info is not set
	 */
        /* francis: yes, but do that before making sure it fits on the screen */
    XtVaGetValues(widget, XmNwidth, &width, XmNheight, &height, 0);
    XtTranslateCoords(widget, 0, 0, &x_root, &y_root_orig);
    x_root += (width/2); /* positon in center of button */
    y_root = y_root_orig + height + 5;

    /* moved here by francis; formerly right before XtVaSetValues(), below */
	if (x_info >= 0 && 
		y_info >= 0) {
		XtTranslateCoords(widget, x_info, y_info, &x_root, &y_root);
	}/* if */
    /*
     *    Make sure it fits on screen.
     */
    XtVaGetValues(parent, XmNborderWidth, &border_width, 0);
    XtVaGetValues(label, XmNwidth, &width, XmNheight, &height, 0);
    screen = XtScreen(label);

    height += (2*border_width);
    width += (2*border_width);

    if (x_root + width > WidthOfScreen(screen))
	x_root = WidthOfScreen(screen) - width;
    else if (x_root < 0)
	x_root = 0;
    if (y_root + height > HeightOfScreen(screen))
	y_root = y_root_orig - height - 5;
    else if (y_root < 0)
	y_root = 0;

	XtVaSetValues(parent, XmNx, x_root, XmNy, y_root, 0);

    /*
     *    Make sure the user cannot shoot themselves with a random
     *    geometry spec. No more attack of the killer tomatoes...djw
     */
    {
		char buf[128];
		sprintf(buf, "%dx%d", width, height);
		XtVaSetValues(parent, XmNwidth, width, XmNheight, height,
					  XmNgeometry, buf, 0);
    }

	/* Mark the tooltips showing */
	fe_tooltip_is_showing = True;

    XtPopup(parent, XtGrabNone);

	/* free s ; from XP_STRDUP
	 */
	XP_FREEIF(s);
    return parent;
}

static void
fe_HTMLTips_stage_two_timeout(XtPointer closure, XtIntervalId *id)
{
	Widget         widget = fe_tool_tips_widget;
    Widget         shell;
	HTMLTipData_t *tipInfo = (HTMLTipData_t *) closure;

    /* Clear the timer id that we store in the context as once the timeout
     * has triggered (that is why we are here), the timeout is automatically
     * removed. Else our event handler will go and remove the timeout again.
     */
    fe_tool_tips_timer_id = 0;

    if (fe_tool_tips_shell == NULL && widget != NULL) {
		shell = fe_HTMLTips_display_stage_two(widget, tipInfo);
		fe_tool_tips_shell = shell;
    }

}

extern void
fe_HTMLViewTooltipsEH(MWContext *context, CL_Layer *layer,
					  CL_Event *layer_event, int state)
{
	Widget widget = CONTEXT_DATA(context)->drawing_area;
    Boolean enter;
    Boolean leave;
    Boolean tips_enabled;

	enter = FALSE;
	leave = FALSE;
	if (state == 2 || state == 3) {
		enter = TRUE;
		leave = TRUE;		
    } /* if */
	else if (state == 1) {
		enter = TRUE;
		leave = FALSE;
	}/* else if */
	else if (state == 4) {
		enter = FALSE;
		leave = TRUE;

    }/* else if */
	else { /* clicks, pops, squeaks, and other non-motionals */
		enter = FALSE;
		leave = FALSE;
    }/* else */
	
    /*
     *    Stage two? Any event should zap that.
     */
    if (fe_tool_tips_shell) {
		/* Mark the tooltips not showing */
		fe_tooltip_is_showing = False;
		XtDestroyWidget(fe_tool_tips_shell);
		fe_tool_tips_shell = NULL;
    }/* if */
    
    /*
     *   Stage one?
     */
    if (fe_tool_tips_timer_id) {
		XtRemoveTimeOut(fe_tool_tips_timer_id);
		fe_tool_tips_timer_id = 0;
    }/* if */

    if (leave == TRUE) {
		fe_tool_tips_widget = NULL;
    }/* if */
    
    if (enter == TRUE) {
		tips_enabled  = fe_globalPrefs.toolbar_tips_p;		
		if (tips_enabled) {
			HTMLTipData_t *tipData = 
				(HTMLTipData_t *) XP_CALLOC(1, sizeof(HTMLTipData_t));
#if DO_NOT_PASS_LAYER_N_EVENT
			LO_Element    *le = NULL;
			int x = layer_event->x,
				y = layer_event->y;

			le = LO_XYToElement(context, x, y, layer);
			if (le && 
				le->type == LO_IMAGE &&
				le->lo_image.alt &&
				le->lo_image.alt_len ) {
				/* do not set time out unless there is an alt_text to display
				 */

				/* to be free by the caller
				 */
				tipData->alt_text = XP_STRDUP((char *)le->lo_image.alt);
				tipData->x = x;
				tipData->y = y;
				tipData->context = context;
				fe_tool_tips_timer_id = 
					XtAppAddTimeOut(fe_XtAppContext,
									500, /* whatever */
									fe_HTMLTips_stage_two_timeout,
									tipData);
			}/* if */
			else
				XP_FREEIF(tipData);
#else

			/* fix BSR in XFE_HTMLView::tipCB; freed there
			 */
			CL_Event *dup = (CL_Event *) XP_CALLOC(1, sizeof(CL_Event));
			dup->x = layer_event->x;
			dup->y = layer_event->y;

			tipData->layer_event = dup;
			tipData->context = context;
			tipData->layer = layer;
			fe_tool_tips_timer_id = 
				XtAppAddTimeOut(fe_XtAppContext,
								500, /* whatever */
								fe_HTMLTips_stage_two_timeout,
								tipData);
#endif
		}/* if tips_enabled */
		fe_tool_tips_widget = widget;
    }/* if */
}

#if HANDLE_LEAVE_WIN
/* HTMLView tooltip eventhanler
 */
static void
fe_HTMLViewTooltips_eh(Widget widget, XtPointer closure, XEvent *event,
					   Boolean *continue_to_dispatch)
{
	MWContext *context = (MWContext *) closure;
	if (widget && 
		context && 
		CONTEXT_DATA(context)&&
		event &&
		widget == CONTEXT_DATA(context)->drawing_area &&
		XfeIsAlive(widget)) {
		switch (event->type) {
		case EnterNotify:
			/* As djw suggested: we treat a window enter as a leave
			 */
			fe_HTMLViewTooltipsEH(context, 
								  (CL_Layer *)NULL, 
								  (CL_Event *)NULL, 4);
			break;

		case LeaveNotify:
			fe_HTMLViewTooltipsEH(context, 
								  (CL_Layer *)NULL, 
								  (CL_Event *)NULL, 4);
			break;

		default:
			break;			
		}/* switch */
	}/* if */
}

extern void fe_Add_HTMLViewTooltips_eh(MWContext *context) 
{
	if (context && 
		CONTEXT_DATA(context)->drawing_area) {
		XtAddEventHandler(CONTEXT_DATA(context)->drawing_area,
						  EnterWindowMask | LeaveWindowMask,
						  FALSE,
						  fe_HTMLViewTooltips_eh,
						  context);
	}/* if */
}

extern void fe_Remove_HTMLViewTooltips_eh(MWContext *context) 
{
	if (context && 
		CONTEXT_DATA(context)->drawing_area) {
		/* send leave to pop down the tooltip
		 */
		fe_HTMLViewTooltipsEH(context, 
							  (CL_Layer *)NULL, 
							  (CL_Event *)NULL, 4);

		XtRemoveEventHandler(CONTEXT_DATA(context)->drawing_area,
							 EnterWindowMask | LeaveWindowMask,
							 FALSE,
							 fe_HTMLViewTooltips_eh,
							 context);
	}/* if */
}
#endif /* HANDLE_LEAVE_WIN */

static void
fe_tooltips_event_handler(Widget widget, XtPointer closure, XEvent* event,
						  Boolean* keep_going)
{
    TipInfo* info = (TipInfo*)closure;
    Boolean enter;
    Boolean leave;
    Widget  child;
    Boolean demo;
    Boolean tips_enabled;

	Boolean isGridWidget = XtIsSubclass(widget, xmlGridWidgetClass);
	int x = event->xbutton.x;
	int y = event->xbutton.y;
	unsigned char rowtype;
	unsigned char coltype;
	int row;
	int column;
	static int m_lastRow = -2;
	static int m_lastCol = -2;
	static Boolean m_inGrid = False;
	static unsigned char  m_lastRowtype = XmALL_TYPES;
	static unsigned char  m_lastColtype = XmALL_TYPES;

   *keep_going = TRUE;
	
    if (info != NULL && info->m_callback == tip_dummy_demo_cb) {
		demo = TRUE;
    } else {
		demo = FALSE;
    }
	
    if (event->type == MotionNotify) {
		enter = FALSE;
		leave = FALSE;
		
		if (info != NULL && info->m_callback == tip_dummy_manager_cb) {
			TipInfo* ginfo;

			child = (Widget)_XM_OBJECT_AT_POINT(widget,
												event->xmotion.x,
												event->xmotion.y);
			
			if (child != NULL
				&& 
				(ginfo = TipInfoListFind(info->m_gadgets, 
										 (XtPointer)child)) != NULL) {
				info = ginfo;
				widget = child;
				if (fe_tool_tips_widget == NULL) {
					leave = FALSE;
					enter = TRUE;
				} else if (fe_tool_tips_widget != widget) {
					leave = TRUE;
					enter = TRUE;
				}
			} else {
				if (fe_tool_tips_widget != NULL) {
					leave = TRUE;
					enter = FALSE;
				}
			}
		} else { /* motion in non-manager widget */
			
			/* Ignore this case if we're dragging.  This fixes
			   tooltips for the ProxyIcon class.  (Otherwise
			   tooltips persist during the drag, bleah.)  CCM */
			
			if(!(event->xmotion.state & Button1Mask)) {
				if (isGridWidget) {
					XmLGridXYToCellTracking(widget, 
								x, y, /* input only args. */
								&m_inGrid, /* input/output args. */
								&m_lastRow, &m_lastCol, 
								&m_lastRowtype, &m_lastColtype,
								&(info->x), &(info->y), /* output only args. */
								&enter, 
								&leave); /* output only args. */

				}/* if  Grid */
				else if (fe_tool_tips_widget == NULL) {
					leave = FALSE;
					enter = TRUE;
				} else if (fe_tool_tips_widget != widget) {
					leave = TRUE;
					enter = TRUE;
				}
			}
		
			if (leave == FALSE && enter == FALSE) /* motion */
				return;
		}
    } else if (event->type == EnterNotify) {
		enter = TRUE;
		leave = FALSE;
		if (isGridWidget) {
			XmLGridXYToCellTracking(widget, 
								x, y, /* input only args. */
								&m_inGrid, /* input/output args. */
								&m_lastRow, &m_lastCol, 
								&m_lastRowtype, &m_lastColtype,
								&(info->x), &(info->y), /* output only args. */
								&enter, 
								&leave); /* output only args. */
			info->rePosition = True;
		}/* if */
     } else if (event->type == LeaveNotify) {
		 enter = FALSE;
		 leave = TRUE;
		 if (isGridWidget) {
			 m_inGrid = False;
			 m_lastRow = m_lastCol = -2;
			 
			 info->x = info->y = -10;
			 info->rePosition = True;
		 }/* if */
	 } else { /* clicks, pops, squeaks, and other non-motionals */
		 enter = FALSE;
		 leave = FALSE;
	 }/* else */
	
    /*
     *    Stage two? Any event should zap that.
     */
    if (fe_tool_tips_shell) {
		/* Mark the tooltips not showing */
		fe_tooltip_is_showing = False;
		XtDestroyWidget(fe_tool_tips_shell);
		fe_tool_tips_shell = NULL;
    }
    
    /*
     *   Stage one?
     */
    if (fe_tool_tips_timer_id) {
		XtRemoveTimeOut(fe_tool_tips_timer_id);
		fe_tool_tips_timer_id = 0;
    }
    
    if (leave == TRUE) {
		if (!isGridWidget)
			/* outliner does not stage_one doc
			 */
			fe_tooltips_display_stage_one(widget, 
										  event, info, FALSE); /* clear */
		fe_tool_tips_widget = NULL;
    }
    
    if (enter == TRUE) {
		if (demo) {
			tips_enabled = XmToggleButtonGetState(widget);
		} else {
			tips_enabled  = fe_globalPrefs.toolbar_tips_p;
		}
		
		if (tips_enabled) {
			fe_tool_tips_timer_id = XtAppAddTimeOut(fe_XtAppContext,
													500, /* whatever */
													fe_tooltips_stage_two_timeout,
													info);
		}
		if (!isGridWidget)
			/* outliner does not stage_one doc
			 */
			fe_tooltips_display_stage_one(widget, event, info, TRUE); /* msg */
		fe_tool_tips_widget = widget;
    }
}

#define TT_EVENT_MASK (PointerMotionMask|EnterWindowMask|LeaveWindowMask| \
					   ButtonPressMask|ButtonReleaseMask| \
					   KeyPressMask|KeyReleaseMask)

void
fe_AddTipStringCallback(Widget widget,
						XtCallbackProc callback, XtPointer closure)
{
	TipInfo* info;
	Boolean  install = False;
	EventMask mask;

	if (XtIsSubclass(widget, xmGadgetClass)) {

		Widget   manager;
		TipInfo* ginfo;

		manager = XtParent(widget);

		if ((info = TipInfoListFind(tips_manager_list,
									(XtPointer)manager)) == NULL) {
			info = TipInfoNewManager(manager);
			install = True;
		}

		ginfo = TipInfoListFind(info->m_gadgets, (XtPointer)widget);

		if (ginfo != NULL) {
			/* print a warning */
			return;
		} else {
			/* create and insert in managers list */
			ginfo = TipInfoNew((XtPointer)widget, callback, closure);
			TipInfoListInsert(&info->m_gadgets, ginfo);
		}

		widget = manager;
		mask = TT_EVENT_MASK;
	} else {

		if (callback != NULL) {
#ifdef USE_WIDGET_LIST
			info = TipInfoListFind(tips_widget_list, widget);
			if (info != NULL) {
				/* print a warning */
				return;
			}
#endif
			info = TipInfoNewWidget(widget, callback, closure);
		} else {
			info = &tips_no_callback_info;
		}
		
		install = True;
		mask = TT_EVENT_MASK;
	}

	if (!install)
		return;
	
    XtRemoveEventHandler(widget,
						 mask,
						 FALSE,
						 fe_tooltips_event_handler,
						 info);
    XtInsertEventHandler(widget,
						 mask,
						 FALSE,
						 fe_tooltips_event_handler,
						 info,
						 XtListHead);
}

static Boolean
fe_manager_tt_default_check(Widget widget)
{
    return (XtIsSubclass(widget, xmPushButtonGadgetClass)
	    ||
	    XtIsSubclass(widget, xmToggleButtonGadgetClass)
	    ||
	    XtIsSubclass(widget, xmCascadeButtonGadgetClass));
}

void
fe_ManagerAddGadgetToolTips(Widget manager, fe_ToolTipGadgetCheckProc do_class)
{
	Widget*  children;
	Cardinal nchildren;
	unsigned n;

	/*
	 *    Now that we keep tipped gadgets on a list, we can use the check
	 *    routine to determien what to install on the manager.
	 */
    if (!do_class)
		do_class = fe_manager_tt_default_check;

	XtVaGetValues(manager,
				  XmNchildren, &children,
				  XmNnumChildren, &nchildren,
				  0);
	
	for (n = 0; n < nchildren; n++) {
		if ((*do_class)(children[n])) {
			fe_AddTipStringCallback(children[n], NULL, NULL);
		}
	}
}

/* Document String Only */
static void fe_docString_hascb_disarm_cb(Widget, XtPointer, XtPointer);
static void fe_docString_hascb_arm_cb(Widget, XtPointer, XtPointer);
static void fe_docString_disarm_cb(Widget, XtPointer, XtPointer);
static void fe_docString_arm_cb(Widget, XtPointer, XtPointer);

/*
 *    These callbacks used if the caller provides no callback.
 *    These guys do not allocate memory, they pass the context around
 *    as the closure.
 */
static void
fe_docString_disarm_cb(Widget w, XtPointer closure, XtPointer call_data)
{
	MWContext* context = (MWContext*)closure;
	XmPushButtonCallbackStruct* cbs = (XmPushButtonCallbackStruct*)call_data;
	TipInfo* info = &tips_no_callback_info;

    fe_display_docString(context, w, info, cbs->event, TRUE);
}

static void
fe_docString_arm_cb(Widget w, XtPointer closure, XtPointer call_data)
{
	MWContext* context = (MWContext*)closure;
	XmPushButtonCallbackStruct* cbs = (XmPushButtonCallbackStruct*)call_data;
	TipInfo* info = &tips_no_callback_info;

    if (!fe_display_docString(context, w, info, cbs->event, FALSE)) {
		/* Arm failed to get any string. Dont waste time from now on for
		 * trying to show the docString everytime we arm this widget.
		 */
		XtRemoveCallback(w, XmNarmCallback, fe_docString_arm_cb, context);
		XtRemoveCallback(w, XmNdisarmCallback, fe_docString_disarm_cb, context);
    }
}

/*
 *    These callbacks used if the caller provides a callback.
 *    These guys do allocate memory, they store the context in the
 *    info, and pass the info around as the closure.
 */
static void
tips_button_death_cb(Widget widget, XtPointer closure, XtPointer cb_data)
{
    TipInfo* info = (TipInfo*)closure;
	XtRemoveCallback(widget, XmNdestroyCallback, tips_button_death_cb, info);
	TipInfoDelete(info);
}

static void
fe_docString_hascb_disarm_cb(Widget w, XtPointer closure, XtPointer call_data)
{
    TipInfo* info = (TipInfo*) closure;
	MWContext* context = (MWContext*)info->m_key;
	XmPushButtonCallbackStruct* cbs = (XmPushButtonCallbackStruct*)call_data;
    fe_display_docString(context, w, info, cbs->event, TRUE);
}

static void
fe_docString_hascb_arm_cb(Widget w, XtPointer closure, XtPointer call_data)
{
    TipInfo* info = (TipInfo*)closure;
	MWContext* context = (MWContext*)info->m_key;
	XmPushButtonCallbackStruct* cbs = (XmPushButtonCallbackStruct*)call_data;

    if (!fe_display_docString(context, w, info, cbs->event, FALSE)) {
		/* Arm failed to get any string. Dont waste time from now on for
		 * trying to show the docString everytime we arm this widget.
		 */
		XtRemoveCallback(w, XmNarmCallback, fe_docString_hascb_arm_cb, context);
		XtRemoveCallback(w, XmNdisarmCallback,
						 fe_docString_hascb_disarm_cb, context);
		tips_button_death_cb(w, (XtPointer)info, call_data);
    }
}

void
fe_ButtonAddDocStringCallback(MWContext* context, Widget widget,
							  XtCallbackProc callback, XtPointer closure)
{
	TipInfo* info = NULL;
	
	if (callback != NULL) {
		info = TipInfoNew((XtPointer)widget, callback, closure);
		info->m_key = (TipInfo*)context;
		XtAddCallback(widget, XmNdestroyCallback, tips_button_death_cb, info);
		XtAddCallback(widget, XmNarmCallback, fe_docString_hascb_arm_cb, info);
		XtAddCallback(widget, XmNdisarmCallback,
					  fe_docString_hascb_disarm_cb, info);
	} else {
		XtAddCallback(widget, XmNarmCallback, fe_docString_arm_cb, context);
		/* Add the disarm callback to erase the document string */
		XtAddCallback(widget, XmNdisarmCallback, fe_docString_disarm_cb, context);
	}
}

void
fe_WidgetAddDocumentString(MWContext *context, Widget widget)
{
	fe_ButtonAddDocStringCallback(context, widget, NULL, NULL);
}


void
fe_WidgetAddToolTips(Widget widget)
{
	fe_AddTipStringCallback(widget, NULL, NULL);
}

Widget
fe_CreateToolTipsDemoToggle(Widget parent, char* name, Arg* args, Cardinal n)
{
    Widget widget;

    widget = XmCreateToggleButton(parent, name, args, n);

	fe_AddTipStringCallback(widget, tip_dummy_demo_cb, NULL);

    return widget;
}

Boolean
fe_ManagerCheckGadgetToolTips(Widget manager, fe_ToolTipGadgetCheckProc check)
{
    Cardinal   nchildren;
    WidgetList children;
    Widget     widget;
    int        i;

    if (!check)
	check = fe_manager_tt_default_check;

    XtVaGetValues(manager,
		  XmNchildren, &children, XmNnumChildren, &nchildren, 0);

    for (i = 0; i < nchildren; i++) {
	widget = children[i];
	if ((*check)(widget) == TRUE)
	    return TRUE;
    }
    return FALSE;
}

Boolean
fe_ContextHasPopups(MWContext* context)
{
    Widget shell = CONTEXT_WIDGET(context);

    return (XfeNumPopups(shell) > 0);
}

/*
 * Motif 1.2 core dumps if right/left arrow keys are pressed while an option
 * menu is active. So always use fe_CreateOptionMenu() instead of
 * XmCreateOptionMenu(). This will create one and set the traversal off
 * on the popup submenu.
 * - dp
 */
Widget
fe_CreateOptionMenu(Widget parent, char* name, Arg* p_argv, Cardinal p_argc)
{
	Widget menu = (Widget )NULL;
	Widget submenu = (Widget )NULL;

	menu = XmCreateOptionMenu(parent, name, p_argv, p_argc);

	XtVaGetValues(menu, XmNsubMenuId, &submenu, 0);
	if (submenu)
	  XtVaSetValues(submenu, XmNtraversalOn, False, 0);

	return menu;
}


/*
 * fe_ProtectContext()
 *
 * Sets the dont_free_context_memory variable in the CONTEXT_DATA. This
 * will prevent the memory for the context and CONTEXT_DATA to be freed
 * even if the context was destroyed.
 */
void
fe_ProtectContext(MWContext *context)
{
  unsigned char del = XmDO_NOTHING;

  if (!CONTEXT_DATA(context)->dont_free_context_memory)
    {
      /* This is the first person trying to protect this context. 
       * Dont allow the user to destroy this context using the windowmanger
       * delete menu item.
       */
      XtVaGetValues(CONTEXT_WIDGET(context), XmNdeleteResponse, &del, 0);
      CONTEXT_DATA(context)->delete_response = del;
      XtVaSetValues(CONTEXT_WIDGET(context),
		    XmNdeleteResponse, XmDO_NOTHING, 0);
    }
  CONTEXT_DATA(context)->dont_free_context_memory++;
}

/*
 * fe_UnProtectContext()
 *
 * Undo what fe_ProtectContext() does. Unsets dont_free_context_memory
 * variable in the context_data.
 */
void
fe_UnProtectContext(MWContext *context)
{
  XP_ASSERT(CONTEXT_DATA(context)->dont_free_context_memory);
  if (CONTEXT_DATA(context)->dont_free_context_memory)
    {
      CONTEXT_DATA(context)->dont_free_context_memory--;
      if (!CONTEXT_DATA(context)->dont_free_context_memory) {
	/* This is the last person unprotecting this context. 
	 * Set the delete_response to what it was before.
	 */
	XtVaSetValues(CONTEXT_WIDGET(context), XmNdeleteResponse,
		      CONTEXT_DATA(context)->delete_response, 0);
      }
    }
}

/*
 * fe_IsContextProtected()
 *
 * Return the protection state of the context.
 */
Boolean
fe_IsContextProtected(MWContext *context)
{
  return (CONTEXT_DATA(context)->dont_free_context_memory);
}

/*
 * fe_IsContextDestroyed()
 *
 * Return if the context was destroyed. This is valid only for protected
 * contexts.
 */
Boolean
fe_IsContextDestroyed(MWContext *context)
{
  return (CONTEXT_DATA(context)->destroyed);
}

/* Recurses over children of context, returning True if any context is
   stoppable. */
Boolean
fe_IsContextLooping(MWContext *context)
{
    int i = 1;
    MWContext *child;
    
    if (!context)
        return False;
    
    if (CONTEXT_DATA(context)->looping_images_p)
        return True;

    while ((child = (MWContext*)XP_ListGetObjectNum (context->grid_children,
													 i++)))
        if (fe_IsContextLooping(child))
            return True;
    
    return False;
}


/* Recurses over children of context, returning True if any context is
   stoppable. */
static Boolean
fe_is_context_stoppable_recurse(MWContext *context)
{
    int i = 1;
    MWContext *child;
    
    if (!context)
        return False;
    
    if ((CONTEXT_DATA(context)->loading_images_p &&
		 CONTEXT_DATA(context)->autoload_images_p) ||
        CONTEXT_DATA(context)->looping_images_p) 
	  return True;

    while ((child = (MWContext*)XP_ListGetObjectNum (context->grid_children,
													 i++)))
        if (fe_is_context_stoppable_recurse(child))
            return True;
    
    return False;
}


/* Returns True if this is a context whose activity can be stopped. */
Boolean
fe_IsContextStoppable(MWContext *context)
{
  /* XP_IsContextStoppable checks for mocha threads, too. */
    return (fe_is_context_stoppable_recurse(context) ||
            XP_IsContextStoppable(context)); 
}


static Widget ToplevelWidget = NULL;


/*
 * FE_SetToplevelWidget
 */
void
FE_SetToplevelWidget(Widget toplevel)
{
    ToplevelWidget = toplevel;
}


/*
 * FE_GetToplevelWidget
 */
Widget
FE_GetToplevelWidget(void)
{
    return ToplevelWidget;
}

Time
fe_GetTimeFromEvent(XEvent* event)
{
	Time time;

	if (event->type == KeyPress || event->type == KeyRelease)
		time = event->xkey.time;
	else if (event->type == ButtonPress || event->type == ButtonRelease)
		time = event->xbutton.time;
	else
		time = XtLastTimestampProcessed(event->xany.display);

	return time;
}

/*
 * Check if Conference is installed 
 */
XP_Bool
fe_IsConferenceInstalled() 
{
	/* Note: will use XP registry to check if conference
	 *       is installed when the installer is ready.
	 *       For now, just check if we can find conference...
	 */

	return (fe_conference_path != NULL);
}

/*
 * Check if Calendar is installed 
 */
XP_Bool
fe_IsCalendarInstalled() 
{
	/* Note: will use XP registry to check if the Calendar component
	 *       is installed when the installer is ready.
	 *       For now, just check if we can find ctime...
	 */

	return (fe_calendar_path != NULL);
}

/*
 * Check if Host on Demand is installed 
 */
XP_Bool
fe_IsHostOnDemandInstalled() 
{
	/* Note: will use XP registry to check if the HOD component
	 *       is installed when the installer is ready.
	 */

	return (fe_host_on_demand_path != NULL);
}
/*
 * Check if polaris is installed
 */
XP_Bool
fe_IsPolarisInstalled() 
{
	return (fe_IsCalendarInstalled() || fe_IsHostOnDemandInstalled());
}


/*
 * fe_IsEditorDisabled
 */
XP_Bool
fe_IsEditorDisabled(void)
{
    XP_Bool disabled = FALSE;

    PREF_GetBoolPref("browser.editor.disabled", &disabled);

    return disabled;
}


/* 
 * Return the URL struct for the brower startup page
 */
URL_Struct *
fe_GetBrowserStartupUrlStruct()
{
	char       *bufp = NULL;
	char       *hist_entry = 0;
	URL_Struct *url = 0;

	if (fe_globalPrefs.browser_startup_page == BROWSER_STARTUP_HOME &&
		fe_globalPrefs.home_document && 
		*fe_globalPrefs.home_document) {
		bufp = XP_STRDUP(fe_globalPrefs.home_document);
	}
	else if (fe_globalPrefs.browser_startup_page == BROWSER_STARTUP_LAST &&
			 fe_ReadLastUserHistory(&hist_entry)) {
		bufp = hist_entry;
	}
	url = NET_CreateURLStruct(bufp, FALSE);
	if (bufp) XP_FREE(bufp);
	return url;
}


/*
 * Get a context that represents the background root window. This is used
 * at init time to find a context we can use to prompt for passwords,
 * confirm or alert before any main window contexts get initialized.
 */
MWContext *
FE_GetInitContext(void)
{
	static MWContext *rootcx = NULL;
	MWContext *m_context = NULL;
	fe_ContextData *fec;
	struct fe_MWContext_cons *cons;
	MWContextType type;
    Widget m_widget;
	LO_Color *color;

	if (rootcx) return rootcx;  

	type = MWContextDialog; /* XXX should be it's own type */

	m_context = XP_NewContext();
	if (m_context == NULL) return NULL;

	fec		= XP_NEW_ZAP (fe_ContextData);
	if (fec == NULL) {
	    XP_DELETE(m_context);
	    return NULL;
        }

	m_context->type = type;
	m_context->is_editor = False;

	CONTEXT_DATA (m_context) = fec;

	/* get cmap.... Set to NULL for nowXXXXX */
	CONTEXT_DATA (m_context)->colormap = NULL;

	m_widget = FE_GetToplevelWidget();
	CONTEXT_WIDGET (m_context) = m_widget;

	fe_InitRemoteServer (XtDisplay (m_widget));

	/* add the layout function pointers */
	m_context->funcs = fe_BuildDisplayFunctionTable();
	m_context->convertPixX = m_context->convertPixY = 1;
	m_context->is_grid_cell = FALSE;
	m_context->grid_parent = NULL;

	/* set the XFE default Document Character set */
	CONTEXT_DATA(m_context)->xfe_doc_csid = fe_globalPrefs.doc_csid;

#ifdef notdef
	fe_InitIconColors(m_context);

	fe_LicenseDialog (m_context);
#endif

	XtGetApplicationResources (m_widget,
							   (XtPointer) CONTEXT_DATA (m_context),
							   fe_Resources, fe_ResourcesSize,
							   0, 0);

#ifdef notdef
	/* Use colors from prefs */
	color = &fe_globalPrefs.links_color;
	CONTEXT_DATA(m_context)->link_pixel = 
		fe_GetPixel(m_context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.vlinks_color;
	CONTEXT_DATA(m_context)->vlink_pixel = 
		fe_GetPixel(m_context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.text_color;
	CONTEXT_DATA(m_context)->default_fg_pixel = 
		fe_GetPixel(m_context, color->red, color->green, color->blue);

	color = &fe_globalPrefs.background_color;
	CONTEXT_DATA(m_context)->default_bg_pixel = 
		fe_GetPixel(m_context, color->red, color->green, color->blue);

	/*
	 * set the default coloring correctly into the new context.
	 */
	{
		Pixel unused_select_pixel;
		XmGetColors (XtScreen (m_widget),
					 fe_cmap(m_context),
					 CONTEXT_DATA (m_context)->default_bg_pixel,
					 &(CONTEXT_DATA (m_context)->fg_pixel),
					 &(CONTEXT_DATA (m_context)->top_shadow_pixel),
					 &(CONTEXT_DATA (m_context)->bottom_shadow_pixel),
					 &unused_select_pixel);
	}
#endif
    rootcx = m_context;
	return rootcx;
}

void fe_CacheWindowOffset(MWContext *context, int32 sx, int32 sy)
{
  CONTEXT_DATA(context)->cachedPos.x=sx;
  CONTEXT_DATA(context)->cachedPos.y=sy;
}

/*
 * Keep track of tooltip mapping to avoid conflict with fascist shells
 * that insist on raising themselves - like taskbar and netcaster webtop
 */
Boolean fe_ToolTipIsShowing(void)
{
	return fe_tooltip_is_showing;
}


/* Get URL for referral if there is one. */
char *fe_GetURLForReferral(History_entry *he)
{
	if (!he)
		return NULL;

    /* Origin URL takes precedence over address */
#ifdef DEBUG_mcafee   /* Waiting for norris checkin */
    if (he && he->origin_url)
        return XP_STRDUP (he->origin_url);
	else
#endif
      if (he && he->address)
		return XP_STRDUP (he->address);
	else
		return NULL;
}
