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
   context_funcs.cpp --- Functions the backend invokes from the 
                         MWContext->funcs vtable
 */


#include "il_types.h"
#include "layers.h"
#include "jsapi.h"
#include "MozillaApp.h"
#include "Frame.h"
#include "ViewGlue.h"
#include "View.h"
#include "HTMLView.h"
#include <Xm/XmP.h>

#ifdef MOZ_MAIL_NEWS
#include "MsgFrame.h"
#include "addrbk.h"
#include "dirprefs.h"
#endif

#include "DisplayFactory.h"
#include "Logo.h"
#include "Dashboard.h"
#include "mozilla.h"
#include "xfe.h"
#include "xp_thrmo.h"
#include "fe_proto.h"

#include "xfe2_extern.h"

#include "Netcaster.h"
#include "NavCenterView.h"

#include "xpgetstr.h"

#if defined(IRIX) || defined(OSF1) || defined(SOLARIS) || defined(SCO_SV) || defined(UNIXWARE) || defined(SNI) || defined(NCR) || defined(NEC)
#include <sys/statvfs.h> /* for statvfs() */
#define STATFS statvfs
#elif defined(HPUX) 
#include <sys/vfs.h>      /* for statfs() */
#define STATFS statfs
#elif defined(LINUX)
#include <sys/vfs.h>      /* for statfs() */
#define STATFS statfs
#elif defined(SUNOS4)
#include <sys/vfs.h>      /* for statfs() */
extern "C" int statfs(char *, struct statfs *);
#define STATFS statfs
#elif defined(BSDI)
#include <sys/mount.h>    /* for statfs() */
#define STATFS statfs
#else
#include <sys/statfs.h>  /* for statfs() */
#define STATFS statfs
extern "C" int statfs(char *, struct statfs *);
#endif

#if defined(OSF1)
extern "C" int statvfs(const char *, struct statvfs *);
#endif

extern int XFE_NO_SUBJECT;
extern int XFE_DOCUMENT_DONE;
extern int XFE_ERROR_SAVING_PASSWORD;
extern int XFE_STDERR_DIAGNOSTICS_HAVE_BEEN_TRUNCATED;
extern int XFE_ERROR_CREATING_PIPE;
extern int XFE_COULD_NOT_DUP_STDERR;
extern int XFE_COULD_NOT_FDOPEN_STDERR;
extern int XFE_COULD_NOT_DUP_A_NEW_STDERR;
extern int XFE_COULD_NOT_DUP_A_STDOUT;
extern int XFE_COULD_NOT_FDOPEN_THE_NEW_STDOUT;
extern int XFE_COULD_NOT_DUP_A_NEW_STDOUT;
extern int MK_UNABLE_TO_LOCATE_FILE;

#ifdef MOZ_LDAP
extern "C" void fe_ldapsearch_finished(MWContext *context);
extern "C" void fe_AB_AllConnectionsComplete(MWContext *context);
#endif

#ifndef NO_WEB_FONTS
/* Webfonts related routines defined in fonts.c */
extern "C" int fe_WebfontsNeedReload(MWContext *context);
#endif /* NO_WEB_FONTS */

#if DEBUG_slamm_
#define D(x) x
#else
#define D(x)
#endif

//
//
// Setup redirection of stdout and stderr to a dialog
//
/* stderr hackery - Why Unix Sucks, reason number 32767.
 */

#include <fcntl.h>

static char stderr_buffer [1024];
static char *stderr_tail = 0;
static time_t stderr_last_read = 0;
static XtIntervalId stderr_dialog_timer = 0;

extern FILE *real_stderr;
FILE *real_stdout = stdout;

static void
fe_stderr_dialog_timer (XtPointer /* closure */, XtIntervalId * /* id */)
{
  char *s = fe_StringTrim (stderr_buffer);
  if (*s)
    {
      /* If too much data was printed, then something has gone haywire,
	 so truncate it. */
      char *trailer = XP_GetString(XFE_STDERR_DIAGNOSTICS_HAVE_BEEN_TRUNCATED);
      int max = sizeof (stderr_buffer) - strlen (trailer) - 5;
      if (strlen (s) > max)
	strcpy (s + max, trailer);

      /* Now show the user.
	 Possibly we should do something about not popping this up
	 more often than every few seconds or something.
       */
      fe_stderr (fe_all_MWContexts->context, s);
    }

  stderr_tail = stderr_buffer;
  stderr_dialog_timer = 0;
}


static void
fe_stderr_callback (XtPointer /* closure */, int *fd, XtIntervalId * /* id */)
{
  char *s;
  int left;
  int size;
  int read_this_time = 0;

  if (stderr_tail == 0)
    stderr_tail = stderr_buffer;

  left = ((sizeof (stderr_buffer) - 2)
	  - (stderr_tail - stderr_buffer));

  s = stderr_tail;
  *s = 0;

  /* Read as much data from the fd as we can, up to our buffer size. */
  if (left > 0)
    {
#if 1
      while ((size = read (*fd, (void *) s, left)) > 0)
	{
	  left -= size;
	  s += size;
	  read_this_time += size;
	}
#else
      size = read (*fd, (void *) s, left);
      left -= size;
      s += size;
      read_this_time += size;
#endif

      *s = 0;
    }
  else
    {
      char buf2 [1024];
      /* The buffer is full; flush the rest of it. */
      while (read (*fd, (void *) buf2, sizeof (buf2)) > 0)
	;
    }

  stderr_tail = s;
  stderr_last_read = time ((time_t *) 0);

  /* Now we have read some data that we would like to put up in a dialog
     box.  But more data may still be coming in - so don't pop up the
     dialog right now, but instead, start a timer that will pop it up
     a second from now.  Should more data come in in the meantime, we
     will be called again, and will reset that timer again.  So the
     dialog will only pop up when a second has elapsed with no new data
     being written to stderr.

     However, if the buffer is full (meaning lots of data has been written)
     then we don't reset the timer.
   */
  if (read_this_time > 0)
    {
      if (stderr_dialog_timer)
	XtRemoveTimeOut (stderr_dialog_timer);

      stderr_dialog_timer =
	XtAppAddTimeOut (fe_XtAppContext, 1 * 1000,
			 fe_stderr_dialog_timer, 0);
    }
}

static void
fe_initialize_stderr (void)
{
  static Boolean done = False;
  int fds [2];
  int in, out;
  int new_stdout, new_stderr;
  int stdout_fd = 1;
  int stderr_fd = 2;
  int flags;

  if (done) return;

  real_stderr = stderr;
  real_stdout = stdout;

#ifdef DEBUG
  // Don't use dialogs for stderr & stdout on debug builds.
  return;
#else
  if (!fe_globalData.stderr_dialog_p && !fe_globalData.stdout_dialog_p)
    return;
#endif

  done = True;

  if (pipe (fds))
    {
      fe_perror (fe_all_MWContexts->context,
                 XP_GetString( XFE_ERROR_CREATING_PIPE) );
      return;
    }

  in = fds [0];
  out = fds [1];

# ifdef O_NONBLOCK
  flags = O_NONBLOCK;
# else
#  ifdef O_NDELAY
  flags = O_NDELAY;
#  else
  ERROR!! neither O_NONBLOCK nor O_NDELAY are defined.
#  endif
# endif

    /* Set both sides of the pipe to nonblocking - this is so that
       our reads (in fe_stderr_callback) will terminate, and so that
       out writes (in net_ExtViewWrite) will silently fail when the
       pipe is full, instead of hosing the program. */
  if (fcntl (in, F_SETFL, flags) != 0)
    {
      fe_perror (fe_all_MWContexts->context, "fcntl:");
      return;
    }
  if (fcntl (out, F_SETFL, flags) != 0)
    {
      fe_perror (fe_all_MWContexts->context, "fcntl:");
      return;
    }

  if (fe_globalData.stderr_dialog_p)
    {
      FILE *new_stderr_file;
      new_stderr = dup (stderr_fd);
      if (new_stderr < 0)
	{
	  fe_perror (fe_all_MWContexts->context,
				 XP_GetString(XFE_COULD_NOT_DUP_STDERR));
	  return;
	}
      if (! (new_stderr_file = fdopen (new_stderr, "w")))
	{
	  fe_perror (fe_all_MWContexts->context,
				 XP_GetString(XFE_COULD_NOT_FDOPEN_STDERR));
	  return;
	}
      real_stderr = new_stderr_file;

      close (stderr_fd);
      if (dup2 (out, stderr_fd) < 0)
	{
	  fe_perror (fe_all_MWContexts->context,
				 XP_GetString(XFE_COULD_NOT_DUP_A_NEW_STDERR));
	  return;
	}
    }

  if (fe_globalData.stdout_dialog_p)
    {
      FILE *new_stdout_file;
      new_stdout = dup (stdout_fd);
      if (new_stdout < 0)
	{
	  fe_perror (fe_all_MWContexts->context,
				 XP_GetString(XFE_COULD_NOT_DUP_A_STDOUT));
	  return;
	}
      if (! (new_stdout_file = fdopen (new_stdout, "w")))
	{
	  fe_perror (fe_all_MWContexts->context,
				 XP_GetString(XFE_COULD_NOT_FDOPEN_THE_NEW_STDOUT));
	  return;
	}
      real_stdout = new_stdout_file;

      close (stdout_fd);
      if (dup2 (out, stdout_fd) < 0)
	{
	  fe_perror (fe_all_MWContexts->context,
				 XP_GetString(XFE_COULD_NOT_DUP_A_NEW_STDOUT));
	  return;
	}
    }

  XtAppAddInput (fe_XtAppContext, in,
		 (XtPointer) (XtInputReadMask /* | XtInputExceptMask */),
		 fe_stderr_callback, 0);
}


// Sanity check on the context.  Throughout the progress stuff below,
// the context (and fe_data) needs to be checked for validity before
// dereferencing the members.
#define CHECK_CONTEXT_AND_DATA(c) \
((c) && CONTEXT_DATA(c) && !CONTEXT_DATA(context)->being_destroyed)

//
// Return either the context's frame or the current active frame
//
XFE_Frame * 
fe_frameFromMWContext(MWContext *context)
{
    XFE_Frame * frame = NULL;

    // Sanity check for possible invocation of this function from a frame
    // that was just destroyed (or is being destroyed)
    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return NULL;
    }

    // Try to use context's frame
    frame = ViewGlue_getFrame(XP_GetNonGridContext(context));
    
	// Try to use the active frame
// 	if (!frame)
// 	{
//  		frame = XFE_Frame::getActiveFrame();
// 	}

	// Make sure the frame is alive
	if (frame && !frame->isAlive())
	{
		frame = NULL;
	}

    return frame;
}

extern "C" void
FE_DestroyWindow(MWContext *context)
{
  XFE_Frame *frame;

  if (!context)
    return;

  /*
   * Recursively destroy any dependent children first
   */
  if (context->js_dependent_list) {
    MWContext *dependent;
    XP_List   *list;

    list = context->js_dependent_list;

    /* NOTE: XP_ListNextObject is a macro which modifies 'list' */
    for( dependent = (MWContext*)XP_ListNextObject(list);
	 dependent;
	 dependent = (MWContext*)XP_ListNextObject(list)) {
      dependent->js_parent = 0; /* prevents recursive call from invoking XP_ListRemoveObject */
      FE_DestroyWindow(dependent);
    }

    /*
     * XP_ListDestroy does *not* free the objects on the list (Apr-97)
     * They are destroyed here by calls to FE_DestroyWindow() above.
     */
    XP_ListDestroy(context->js_dependent_list);
    context->js_dependent_list = 0;
  }

  /*
   * This code is invoked when a dependent window is not being destroyed by
   * its parent.
   */
  if (context->js_parent && context->js_parent->js_dependent_list)
    XP_ListRemoveObject(context->js_parent->js_dependent_list, context);


  /*
   * Now destroy the window
   */
  frame = ViewGlue_getFrame(XP_GetNonGridContext(context));
  XP_ASSERT(frame);
  if (frame == NULL) return;

  frame->app_delete_response();

}

extern "C" void
XFE_EnableClicking(MWContext *context)
{
  MWContext *top = XP_GetNonGridContext (context);
  XFE_Frame *f;
  XP_Bool running_p;

  // this is very evil but has been known to happen
  if (top == NULL) return;

  f = ViewGlue_getFrame(top);

  running_p = XP_IsContextBusy (top);

  D( printf("XFE_EnableClicking(context = 0x%x) running_p = %d\n",
            context, running_p); )

  if (f)
    {
      f->notifyInterested(XFE_View::commandNeedsUpdating,
						   (void*)xfeCmdStopLoading);

	  f->notifyInterested(running_p ?
						  XFE_Frame::frameBusyCallback:
						  XFE_Frame::frameNotBusyCallback);
    }
  else
    {
      if (CONTEXT_DATA (top)->show_toolbar_p && CONTEXT_DATA (top)->toolbar) {
	if (CONTEXT_DATA (top)->abort_button)
	  XtVaSetValues (CONTEXT_DATA (top)->abort_button,
			 XmNsensitive, fe_IsContextStoppable(top), 0);
      }
      if (CONTEXT_DATA (top)->show_menubar_p && CONTEXT_DATA (top)->menubar) {
	if (CONTEXT_DATA (top)->abort_menuitem)
	  XtVaSetValues (CONTEXT_DATA (top)->abort_menuitem,
			 XmNsensitive, fe_IsContextStoppable(top), 0);
      }
    }

  if (! running_p)
    fe_StopProgressGraph (context);

  if (CONTEXT_DATA (context)->clicking_blocked)
    {
      CONTEXT_DATA (context)->clicking_blocked = False;
      /* #### set to link cursor if over link. */
      fe_SetCursor (context, False);
    }
}

extern "C" void
FE_UpdateStopState(MWContext *context)
{
  MWContext *top = XP_GetNonGridContext (context);
  XFE_Frame *f;

  // this is very evil but has been known to happen
  if (top == NULL) return;

  f = ViewGlue_getFrame(top);

  if (f)
    {
D( printf("FE_UpdateStopState(context = 0x%x)\n", context); )

      f->notifyInterested(XFE_View::commandNeedsUpdating,
						   (void*)xfeCmdStopLoading);
	  
	  // Brendan says this should check for stoppable, not busyness...mcafee
	  // But, I talked to him, and he agreed that I was right all along..djw
	  // Save time: trust David. hee hee. Ok, here is the scoop. Because,
	  // this call is made in FE_UpdateStopState(), which is really exists
	  // to control the stop button, so you'd think all the stuff in here
	  // should be using XP_IsContextStoppable(). Fair enough. The thing is
	  // this call to the busy notification dependencies (notably the cursor),
	  // has nothing to do with the stop button, etc. It's just that this
	  // is a good salient point to update the cursor. So, calling 
	  // XP_IsContextBusy() is the right thing. Either that or take the whole
	  // thing out, say "what was I trying", and get on with your
	  // life. Hmmm, actually that sounds pretty good, cheers...djw
	  f->notifyInterested(XP_IsContextBusy(context)?
						  XFE_Frame::frameBusyCallback:
						  XFE_Frame::frameNotBusyCallback);
    }
  else
    {
      if (context->type == MWContextPostScript) return;
      
      if (CONTEXT_DATA (top)->show_toolbar_p && CONTEXT_DATA (top)->toolbar) {
	if (CONTEXT_DATA (top)->abort_button)
	  XtVaSetValues (CONTEXT_DATA (top)->abort_button,
			 XmNsensitive, fe_IsContextStoppable(top), 0);
      }
      if (CONTEXT_DATA (top)->show_menubar_p && CONTEXT_DATA (top)->menubar) {
	if (CONTEXT_DATA (top)->abort_menuitem)
	  XtVaSetValues (CONTEXT_DATA (top)->abort_menuitem,
			 XmNsensitive, fe_IsContextStoppable(top), 0);
      }
    }
}

extern "C" void
XFE_AllConnectionsComplete(MWContext *context)
{
  MWContext *top = XP_GetNonGridContext (context);
  XFE_Frame *f;

  // this is evil and vile but it's been known to happen
  if (top == NULL) return;

  f = ViewGlue_getFrame(top);

  if (CONTEXT_DATA (context)->being_destroyed) return;

  D( printf("XFE_AllConnectionsComplete(context = 0x%x)\n",context); )

  /* #### ARRGH, when generating PostScript, this assert gets tripped!
     I guess this is because of the goofy games it plays with invoking
     callbacks in the "other" context...
     assert (CONTEXT_DATA (context)->active_url_count == 0);
     */
  
  /* If a title was never specified, clear the old one. */
  if ((context->type != MWContextBookmarks) &&
      (context->type != MWContextAddressBook) &&
      (context->type != MWContextSearch) &&
      (context->type != MWContextSearchLdap) &&
      (CONTEXT_DATA (context)->active_url_count == 0) &&
      (context->title == 0))
    XFE_SetDocTitle (context, 0);
  
  /* This shouldn't be necessary, but it doesn't hurt. */
  XFE_EnableClicking (context);
  
  fe_RefreshAllAnchors ();
  
  /* If either the user resized during layout or there was a webfont that
	 was being downloaded while rendering the page, reload the document right
	 now (it will come from the cache.) */
#ifndef NO_WEB_FONTS
  if (fe_WebfontsNeedReload(context))
	{
	  CONTEXT_DATA (context)->relayout_required = False;
	  LO_InvalidateFontData(context);
	  fe_ReLayout (context, NET_RESIZE_RELOAD);
	}
  else
#endif /* NO_WEB_FONTS */
  if (CONTEXT_DATA (context)->relayout_required)
    {
      if (! XP_IsContextBusy (context))
	{
	  CONTEXT_DATA (context)->relayout_required = False;
        /*  As vidur suggested in Bug 59214: because JS generated content
        was not put into wysiwyg, hence this source was not be shown on a resize.
        The fix was to make the reload policy for Mail/News contexts NET_NORMAL_RELOAD.
        This would fix exceed server problem too. Exceed server sends redraw events on
	each resize movement */
        if ( (context->type == MWContextNews) 
	     || (context->type == MWContextMail)
             || (context->type == MWContextNewsMsg) 
	     || (context->type == MWContextMailMsg) )
          fe_ReLayout (context, NET_NORMAL_RELOAD);
        else
          fe_ReLayout (context, NET_RESIZE_RELOAD);
	}
    }
  else if (context->is_grid_cell && CONTEXT_DATA (top)->relayout_required)
    {
      if (! XP_IsContextBusy (top))
	{
	  CONTEXT_DATA (top)->relayout_required = False;
          /*  As vidur suggested in Bug 59214: because JS generated content
          was not put into wysiwyg, hence this source was not be shown on a resize.
          The fix was to make the reload policy for Mail/News contexts NET_NORMAL_RELOAD.
          This would fix exceed server problem too. Exceed server sends redraw events on
	  each resize movement */
          if ( (context->type == MWContextNews) 
	     || (context->type == MWContextMail)
             || (context->type == MWContextNewsMsg) 
	     || (context->type == MWContextMailMsg) )
            fe_ReLayout (top, NET_NORMAL_RELOAD);
          else
            fe_ReLayout (top, NET_RESIZE_RELOAD);
	}
    }
  else if((context->type != MWContextSearch) 
	&& (context->type != MWContextSearchLdap) )
    {
      XFE_Progress (top, XP_GetString(XFE_DOCUMENT_DONE));
    }
  
  if (f && f->isAlive())
    {
      f->allConnectionsComplete();
    }
  else
    {
#if 0
      printf ("ConnectionsComplete: 0x%x; grid: %d; busy: %d, relayout: %d\n",
	      context, context->is_grid_cell, XP_IsContextBusy (context),
	      CONTEXT_DATA (context)->relayout_required);
#endif
      
      if (context->type == MWContextSaveToDisk) /* gag gag gag */
	fe_DestroySaveToDiskContext (context);
      else if ( context->type == MWContextSearchLdap) 
	{
#ifdef UNIX_LDAP 
//	  fe_ldapsearch_finished(context);
#endif
	}
#ifdef MOZ_LDAP
      else if ( context->type == MWContextAddressBook) /* AB ldap search */
	{
	  fe_AB_AllConnectionsComplete(context);
	}
#endif  // MOZ_LDAP
    }
}

/* Prompt the user for a string.
   If the user selects "Cancel", 0 is returned.
   Otherwise, a newly-allocated string is returned.
   A pointer to the prompt-string is not retained.
 */
extern "C" char *
XFE_Prompt (MWContext *context, const char *message, const char *deflt)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
    return f->prompt("prompt", message, deflt);
  else
    return (char *) fe_prompt (context, CONTEXT_WIDGET (context),
                               "prompt", message,
                               TRUE, (deflt ? deflt : ""),
                               TRUE, FALSE, 0);
}

extern "C" char *
XFE_PromptWithCaption(MWContext *context, const char *caption,
		      const char *message, const char *deflt)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
    return f->prompt(caption, message, deflt);
  else
    return (char *) fe_dialog(CONTEXT_WIDGET(context), caption, message,
			      TRUE, (deflt ? deflt : ""), TRUE, FALSE, 0);

}

extern "C" char *
FE_PromptMessageSubject (MWContext *context)
{
  const char *def = XP_GetString( XFE_NO_SUBJECT );

  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
    return f->prompt("promptSubject", 0, def);
  else
    return (char *) fe_dialog (CONTEXT_WIDGET (context), "promptSubject",
			       0, TRUE, def, TRUE, TRUE, 0);
}

// Some History functions that we need to implement here now.
extern "C" int
FE_EnableBackButton (MWContext *context)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
    f->notifyInterested(XFE_View::commandNeedsUpdating, (void*)xfeCmdBack);

  return 0;
}

extern "C" int
FE_DisableBackButton (MWContext *context)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
    f->notifyInterested(XFE_View::commandNeedsUpdating, (void*)xfeCmdBack);

  return 0;
}

extern "C" int
FE_EnableForwardButton (MWContext *context)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
    f->notifyInterested(XFE_View::commandNeedsUpdating, (void*)xfeCmdForward);

  return 0;
}

extern "C" int
FE_DisableForwardButton (MWContext *context)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
    f->notifyInterested(XFE_View::commandNeedsUpdating, (void*)xfeCmdForward);

  return 0;
}

extern "C" void
fe_MidTruncatedProgress(MWContext *context, const char *message)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
	  {
		  if (message == 0 || *message == '\0')
			  message = context->defaultStatus;

		  f->notifyInterested(XFE_View::statusNeedsUpdatingMidTruncated, (void*)message);
	  }
}

extern "C" void
fe_SetURLString (MWContext *context, URL_Struct *url)
{
  XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (f)
    f->notifyInterested(XFE_HTMLView::newURLLoading, url);
}

typedef enum {
	Answer_Invalid = -1,
	Answer_Cancel = 0,
	Answer_OK,
	Answer_Apply,
	Answer_Destroy 
} Answers;

struct fe_fileprompt_data {
	Answers answer;
	char *return_value;
	Boolean must_match;
};

static void
fe_fileprompt_finish_cb (Widget /*widget*/, XtPointer closure, XtPointer /*call_data*/)
{
  struct fe_fileprompt_data *data = (struct fe_fileprompt_data *) closure;
  if (data->answer == Answer_Invalid)
	  data->answer = Answer_Destroy;
}

static void
fe_fileprompt_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
	fe_fileprompt_data *data = (fe_fileprompt_data *)closure;
	XmFileSelectionBoxCallbackStruct *sbc = (XmFileSelectionBoxCallbackStruct *) call_data;

	switch (sbc->reason)
	{
    case XmCR_NO_MATCH:
      {
	  NOMATCH:
		  XBell (XtDisplay (widget), 0);
		  break;
      }
	case XmCR_OK:
		{
			XmStringGetLtoR (sbc->value, XmFONTLIST_DEFAULT_TAG,
							 (char **) &data->return_value);
			/* mustMatch doesn't work!! */
			{
				struct stat st;
				if (data->must_match &&
					data->return_value &&
					stat (data->return_value, &st)){
					free (data->return_value);
					data->return_value = 0;
					goto NOMATCH;
				}
			}
			data->answer = Answer_OK;
			break;
		}
	case XmCR_CANCEL:
		{
			data->answer = Answer_Cancel;
			data->return_value = 0;
			break;
		}
	default:
		abort ();
	}
}

extern "C" char* 
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


/*
 * Status of filename prompting:
 * We have three routines, FE_PromptForFileName() here, and
 * fe_ReadFileName() and fe_ReadFileName_2() in dialogs.c.
 * FE_PromptForFileName() is the one called by the backend;
 * the other two are called at various points in the XFE.
 * fe_ReadFileName() immediately calls fe_ReadFileName_2().
 * We need to integrate these three routines into one!
 *      ...Akkana 10/20/97
 */
extern "C" int
FE_PromptForFileName (MWContext *context,
		      const char *prompt_string,
		      const char *default_path,
		      XP_Bool file_must_exist_p,
		      XP_Bool directories_allowed_p,
		      ReadFileNameCallbackFunction fn,
		      void *closure)
{
#if 0
    // Note: this needs to be changed somehow to call fe_ReadFileName_2 ...
  char *file = fe_ReadFileName (context, prompt_string, default_path,
				file_must_exist_p, 0);
  if (!file)
    return -1;

  fn (context, file, closure);
  return 0;
#endif

  Widget    parent;
  Widget    shell;
  Widget    fileb;
  Visual   *v = 0;
  Colormap  cmap = 0;
  Cardinal  depth = 0;
  char     *text = 0;
  char     *orig_text = 0;
  char      buf[1024];
  XmString  xmpat = 0;
  XmString  xmfile = 0;
  Arg       av[20];
  int       ac;
  struct    fe_fileprompt_data data;

  /* relative filename
   */
  XmString dirMask;
  XP_Bool relative_filename = False;

  parent = CONTEXT_WIDGET(context);

  XtVaGetValues(parent,
				XtNvisual, &v,
				XtNcolormap, &cmap,
				XtNdepth, &depth, 0);

  if (default_path && *default_path) {	
	  StrAllocCopy(text, default_path);
	  orig_text = text;
  }

  if (text && *text)
	  text = XP_STRTOK(text, " ");
 
  if (!text || !*text) {
      xmpat = 0;
      xmfile = 0;
  }
  else if (directories_allowed_p) {
      if (text[XP_STRLEN(text) - 1] == '/')
		  text[XP_STRLEN(text) - 1] = 0;
      PR_snprintf(buf, sizeof(buf), "%.900s/*", text);
      xmpat = XmStringCreateLtoR(buf, XmFONTLIST_DEFAULT_TAG);
      xmfile = XmStringCreateLtoR(text, XmFONTLIST_DEFAULT_TAG);
  }
  else {
      char *f;
      if (text[XP_STRLEN(text) - 1] == '/')
		  PR_snprintf(buf, sizeof (buf), "%.900s/*", text);
      else
		  PR_snprintf(buf, sizeof (buf), "%.900s", text);
      xmfile = XmStringCreateLtoR(buf, XmFONTLIST_DEFAULT_TAG);
      if (text[0] == '/') /* only do this for absolute path */
		  f = XP_STRRCHR(text, '/');
      else
		  f = NULL;

      if (f && f != text)
		  *f = 0;

      if (f) {
		  PR_snprintf(buf, sizeof (buf), "%.900s/*", text);
		  xmpat = XmStringCreateLtoR(buf, XmFONTLIST_DEFAULT_TAG);
	  }
      else {
		  relative_filename = True;
	  }
  }
  ac = 0;
  XtSetArg(av[ac], XmNvisual, v); ac++;
  XtSetArg(av[ac], XmNdepth, depth); ac++;
  XtSetArg(av[ac], XmNcolormap, cmap); ac++;
  XtSetArg(av[ac], XmNdeleteResponse, XmDESTROY); ac++;
  shell = XmCreateDialogShell(parent, "fileBrowser_popup", av, ac);

  ac = 0;
  XtSetArg(av[ac], XmNdialogStyle, XmDIALOG_PRIMARY_APPLICATION_MODAL); ac++;
  XtSetArg(av[ac], XmNfileTypeMask,
		   (directories_allowed_p ? XmFILE_DIRECTORY : XmFILE_REGULAR)); ac++;
  fileb = fe_CreateFileSelectionBox(shell, "fileBrowser", av, ac);

  XtAddCallback (fileb, XmNokCallback,      fe_fileprompt_cb, &data);
  XtAddCallback (fileb, XmNcancelCallback,  fe_fileprompt_cb, &data);
  XtAddCallback (fileb, XmNdestroyCallback, fe_fileprompt_finish_cb, &data);

  if (prompt_string && *prompt_string) {
	  XtVaSetValues(shell, XmNtitle, prompt_string, 0);
  }

  XtVaSetValues (fileb, XmNmustMatch, False, 0);

  if (xmpat && !relative_filename) {
      XtVaSetValues(fileb,
					XmNdirMask, xmpat,
					XmNpattern, xmpat, 
					XmNdirSpec, xmfile, 
					0);
      XmStringFree(xmpat);
      XmStringFree(xmfile);
  }
  else if (text) {
	  char *basename = XP_STRRCHR(text, '/');
	  int len = 0;
	  if (!basename) {
		  basename = text;
		  len=XP_STRLEN(basename);
	  }/* if */
	  else if ((len=XP_STRLEN(basename)) > 1)
		  basename++;
			  
	  XmString xm_directory;
	  XtVaGetValues(fileb, XmNdirectory, &xm_directory, 0);

	  String dir_part = NULL;
	  if (xm_directory != NULL)
		  dir_part = _XmStringGetTextConcat(xm_directory);

	  if (dir_part) {
		  int dir_len = XP_STRLEN(dir_part);
		  len += dir_len;
		  if (dir_part[dir_len-1] != '/')
			  len++;
		  char *filename = (char *) XP_CALLOC(len+1, sizeof(char));
		  XP_STRCAT(filename, dir_part);
		  if (dir_part[dir_len-1] != '/')
			  XP_STRCAT(filename, "/");
		  if (basename)
			  XP_STRCAT(filename, basename);
		  xmfile = XmStringCreateLtoR(filename, XmFONTLIST_DEFAULT_TAG);
	  }/* if */
	  XtVaGetValues (fileb, XmNdirMask, &dirMask, 0);

	  XmFileSelectionDoSearch (fileb, dirMask);
	  XtVaSetValues(fileb, XmNdirSpec, xmfile, 0);
      XmStringFree(xmfile);
  }/* else */
  if (orig_text) free(orig_text);


  data.answer = Answer_Invalid;
  data.return_value = 0;
  data.must_match = file_must_exist_p;

  fe_HackDialogTranslations (fileb);

  fe_NukeBackingStore (fileb);

  fe_UnmanageChild_safe (XmSelectionBoxGetChild (fileb, XmDIALOG_HELP_BUTTON));
  XtManageChild (fileb);

  /* check for destruction here */
loop:
  while (data.answer == Answer_Invalid)
	  fe_EventLoop ();

  if (data.answer == Answer_OK) {
      // Check for invalid directory:
      char* dirname = XP_STRDUP(data.return_value);
      char* basename = XP_STRRCHR(dirname, '/');
      DIR* dirp;

      if (basename != 0) {
          *basename = 0;
          dirp = opendir(dirname);
          if (dirp == 0) {
              char s[256];
              sprintf(s, XP_GetString(MK_UNABLE_TO_LOCATE_FILE), dirname);
              FE_Alert(context, s);
              if (data.return_value)
                  free(data.return_value);
              XP_FREE(dirname);
              data.answer = Answer_Invalid;
              goto loop;
          }
          closedir(dirp);
          XP_FREE(dirname);
      }

	  if (data.return_value) {
		  fn(context, data.return_value, closure);
		  free(data.return_value);
      }
  }

  if (data.answer != Answer_Destroy)
	  XtDestroyWidget(shell);
  
  return (data.answer== Answer_OK && data.return_value) ? 0 : -1;
}

extern "C" void
FE_QueryChrome(MWContext * context,  /* in */
	       Chrome * chrome) /* out */

/**>
 *
 * description:
 *	Fill in a chrome structure, given a context.
 *
 * returns:
 *	n/a.  The chrome parameter is modified.
 *
 */
{
  XFE_Frame *       frame;

  if (!chrome || !context || context->type != MWContextBrowser)
    return;

  frame = ViewGlue_getFrame(XP_GetNonGridContext(context));
  if (frame == NULL) return;

  frame->queryChrome(chrome);

  // if this is a grid cell, fix w_hint & h_hint
  if (context->is_grid_cell) {
    chrome->w_hint = (int32) CONTEXT_DATA (context)->scrolled_width;
    chrome->h_hint = (int32) CONTEXT_DATA (context)->scrolled_height;
#ifdef DEBUG_spence
    printf ("QueryChrome: grid cell: w = %d, h = %d\n",
	    chrome->w_hint, chrome->h_hint);
#endif    
  }
}



extern "C" void
FE_UpdateChrome(MWContext *context, /* in */
		Chrome *chrome) /* in */

/**
 *
 * description:
 *	Update a frame based on the chrome.
 *
 * returns:
 *	n/a
 *
 */
{
  XFE_Frame * frame;
  Widget      w;

  if (!chrome || !context || context->type != MWContextBrowser)
    return;

  w = CONTEXT_WIDGET(context);
  if (!XtIsManaged(w))
    XtManageChild(w);

  frame = ViewGlue_getFrame(XP_GetNonGridContext(context));
  if (frame == NULL) return;

  frame->respectChrome(chrome);
  frame->doAttachments();
}

#ifdef MOZ_LDAP
extern "C" void
FE_RememberPopPassword (MWContext *context, const char *password)
{
  /* Store password into preferences. */
  StrAllocCopy (fe_globalPrefs.pop3_password, password);

  /* If user has already requesting saving it, do that. */
  if (fe_globalPrefs.rememberPswd)
    {
      if (!XFE_SavePrefs ((char *) fe_globalData.user_prefs_file,
			  &fe_globalPrefs))
	fe_perror (context, XP_GetString( XFE_ERROR_SAVING_PASSWORD ) );
    }
}
#endif

extern "C" void
FE_BackCommand(MWContext *context)
{
        XFE_Frame *frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

        if (!frame)
                return;

        if (frame->handlesCommand(xfeCmdBack)
                && frame->isCommandEnabled(xfeCmdBack))
                frame->doCommand(xfeCmdBack);
}

extern "C" void
FE_ForwardCommand(MWContext *context)
{
        XFE_Frame *frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

        if (!frame)
                return;

        if (frame->handlesCommand(xfeCmdForward)
                && frame->isCommandEnabled(xfeCmdForward))
                frame->doCommand(xfeCmdForward);
}

extern "C" void
FE_HomeCommand(MWContext *context)
{
        XFE_Frame *frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

        if (!frame)
                return;

        if (frame->handlesCommand(xfeCmdHome)
                && frame->isCommandEnabled(xfeCmdHome))
                frame->doCommand(xfeCmdHome);
}

extern "C" void
FE_PrintCommand(MWContext *context)
{
        XFE_Frame *frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

        if (!frame)
                return;

        if (frame->handlesCommand(xfeCmdPrint)
                && frame->isCommandEnabled(xfeCmdPrint))
                frame->doCommand(xfeCmdPrint);
}

extern "C" void
FE_OpenFileCommand(MWContext*context)
{
        XFE_Frame *frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

        if (!frame)
                return;

        if (frame->handlesCommand(xfeCmdOpenPageChooseFile)
                && frame->isCommandEnabled(xfeCmdOpenPageChooseFile))
                frame->doCommand(xfeCmdOpenPageChooseFile);
}

extern "C" uint32
FE_DiskSpaceAvailable (MWContext* /*context*/, const char* filename)
{
  char curdir [MAXPATHLEN];
  struct STATFS fs_buf;

  if (!filename || !*filename) {
    (void) getcwd (curdir, MAXPATHLEN);
    if (! curdir) return 1L << 30;  /* hope for the best as we did in cheddar */
  } else {
    PR_snprintf (curdir, MAXPATHLEN, "%.200s", filename);
  }

  if (STATFS (curdir, &fs_buf) < 0) {
    /*    fe_perror (context, "Cannot stat current directory\n"); */
    return 1L << 30; /* hope for the best as we did in cheddar */
  }

#ifdef DEBUG_DISK_SPACE
  printf ("DiskSpaceAvailable: %d bytes\n", 
	  fs_buf.f_bsize * (fs_buf.f_bavail - 1));
#endif
  return (fs_buf.f_bsize * (fs_buf.f_bavail - 1));
}


/*
 * I found this function reporting the document's scroll offset, rather
 *  than the window's location.  In fixing it, I found that XGetGeometry()
 *  didn't work--it always reported (0,0)--and any approach that would
 *  work would necessarily involve a round-trip to the X server; so,
 *  instead, I decided to cache the difference between (x_root,y_root) and
 *  (x,y) in incoming events.  The cachedPos is updated by
 *  fe_CacheWindowOffset() (see xfe.h and xfe.c), which is called from
 *  fe_stuff_event(), and should maybe be called from other places that
 *  may receive button or motion events.
 *
 * It is possible for this information to get out of sync with reality.
 *  If FE_GetWindowOffset() is called at a time when it's been a while
 *  since some event that triggers fe_CacheWindowOffset(), then it may
 *  be wrong.  My hypothesis is that FE_GetWindowOffset() isn't called
 *  in too many place (I discovered it was wrong in the context of fixing
 *  JS events; but, in theory, JS events will be triggered by X events,
 *  which should trigger fe_CacheWindowOffset() first).
 *
 * -- francis, 9 May 1997
 */
extern "C" void
FE_GetWindowOffset(MWContext *context, int32 *sx, int32 *sy)
{
  (*sx)=CONTEXT_DATA(context)->cachedPos.x;
  (*sy)=CONTEXT_DATA(context)->cachedPos.y;
}

extern "C" void
FE_GetScreenSize(MWContext *context, int32 *sx, int32 *sy)
{
	XFE_Frame *frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

	if (frame == NULL) return;

	*sx = WidthOfScreen(XtScreen(frame->getBaseWidget()));
	*sy = HeightOfScreen(XtScreen(frame->getBaseWidget()));
}

/*******************************************************************
 * 
 */

extern "C" void
FE_GetAvailScreenRect(MWContext *pContext, /* in */
		      int32 *sx,   /* out */
		      int32 *sy,   /* out */
		      int32 *left, /* out */
		      int32 *top)  /* out */

/*
 *
 * description:
 *	This function obtains the rectangular area of the screen available
 *	for use by Mozilla & Co.
 *	On Unix this is the entire screen (implemented here).
 *	On Macintosh this is the screen less the area for the menubar.
 *	On Win95 and Win NT 4.0 this is the screen less area occupied
 *	by the taskbar.
 *
 * preconditions:
 *	All arguments must be non-NULL
 *
 * returns:
 *	Places the dimensions of the screen in sx, sy, left and top
 *	
 ****************************************/
{
  *left = 0;
  *top  = 0;
  FE_GetScreenSize(pContext, sx, sy);
}


/*
 * BUG 57147 June 1997
 *
 * A fix for this bug was implemented which disturbs the existing
 * code as little as possible at the cost of being kludgy.  It would
 * be nicer to rearchitect all of the find code, but we're too close
 * to shipping Communicator 4.0 now.
 *
 * For the curious, here is a description of what is going on.
 *
 * JAVASCRIPT FIND() COMMAND
 * In Navigator 3.0 days it was only possible to invoke find from the
 * user interface.  One brought up the "find" dialog from a menu, after which
 * one could invoke the "find again" menu item.  With
 * Communicator 4.0 it is now possible to invoke the find() function
 * from JavaScript, e.g.
 *	find();      // bring up the find dialog box, aka interactive find
 *	find("cat"); // search for "cat" with no dialog, aka non-interactive find
 * Another complication is that it must be possible from JavaScript
 * to invoke find on a named html frame.  In the past it was assumed
 * that a find was to be performed on the frame with focus.  With
 * Communicator 4.0 it must be possible to perform a find regardless
 * of whether or not a frame has focus.
 *
 * IMPLEMENTATION
 * fe_find_cb() is invoked whenever the user chooses the find menu item.
 * fe_find_again_cb() is invoked whenever the user chooses the find again
 * menu item.
 * FE_FindCommand() is invoked for all calls to JavaScript find().
 * The lower level find functions fe_find_refresh_data() and fe_FindDialog()
 * which eventually get called from both the front end and FE_FindCommand()
 * use fe_findcommand_context() to determine if they are being called
 * by JavaScript or by the front end.  A call to FE_FindCommand() ensures that
 * fe_findcommand_context() returns non-NULL.  Calls to fe_find_cb() and
 * fe_find_again_cb() ensure that fe_find_command_context() returns NULL.
 *
 */


/*
 *  Written to by FE_FindCommand() and fe_unset_findcommand_context()
 */
static MWContext * private_fe_findcommand_context = 0;


/****************************************
 *
 */

extern "C" MWContext * fe_findcommand_context()

/*
 * returns:
 *	NULL whenever a find is being performed after
 *	fe_find_cb() and fe_find_again_cb() have been called.
 *	This means that these callback functions call
 *	fe_unset_findcommand_context().  Lower level find
 *	functions such as fe_find_refresh_data() and fe_FindDialog()
 *	determine the context which currently has focus to do the find.
 *
 *	non-NULL whenever a find is being performed as a result
 *	of FE_FindCommand().  Lower level find functions should
 *	use this context for the find.
 *
 ****************************************/
{
  return private_fe_findcommand_context;
}


/****************************************
 *
 */

extern "C" void fe_unset_findcommand_context()

/*
 * description:
 *	Called by fe_find_cb() and fe_find_again_cb()
 *	so that lower level functions do the same thing
 *	as they did in the past when find and find again
 *	menu items are chosen.
 *
 ****************************************/
{
  private_fe_findcommand_context = 0;
}



/****************************************
 *
 */

extern "C" XP_Bool
FE_FindCommand(MWContext *context,          /* in MOD: can this be mail or news? */
	       char *  search_string,       /* in: NULL brings up dialog */
	       XP_Bool b_is_case_sensitive, /* in */
	       XP_Bool b_search_backwards,  /* in */
	       XP_Bool /* b_wrap */)             /* ignored */

/*
 * description:
 *	Implemented to support the JavaScript
 *	find() function for Communicator 4.0.
 *
 *	The portion of this function which executes non-interactive
 *	searches endeavors to reproduce the relevant portions of
 *	the code which would be executed by calling
 *	frame->doCommand(xfeCmdFindAgain) without creating the find
 *	dialog box widgets.
 *
 * side effects:
 *	The fe_FindData member of the relevant context will be
 *	allocated if it doesn't already exist.
 *
 * returns:
 *	If search_string is NULL, always returns FALSE
 *	otherwise returns TRUE or FALSE depending on whether or not
 *	the string was found.
 *
 * comments:
 *	No meaningful return value is currently (Apr-97) possible
 *	when a dialog is brought up because the function immediately
 *	returns without waiting for the user to perform a find.
 *	Note that the user may cancel or do any number of searches
 *	before closing the dialog.
 *
 ****************************************/
{
  XP_Bool     result = FALSE;
  XFE_Frame * frame;
  CL_Layer  * layer;

  frame = ViewGlue_getFrame(XP_GetNonGridContext(context));
  if (!frame)
    return result;


  private_fe_findcommand_context = context;

  /*
   *
   * BRING UP FIND DIALOG AKA FIND IN OBJECT
   *
   * if no search string is specified,
   * bring up a find dialog aka "find in object"
   * and return FALSE
   */
  if (!search_string) {
    if (frame->handlesCommand(xfeCmdFindInObject)
	&& frame->isCommandEnabled(xfeCmdFindInObject)) {
      /* code adapted from commands.c:fe_find_cb */
      MWContext * top_context;

      top_context = XP_GetNonGridContext(context);
      if (!top_context)
	top_context = context;

      fe_UserActivity (top_context);
      fe_FindDialog(top_context, False);
    }
  }
  /*
   * NON-INTERACTIVE SEARCH AKA FIND AGAIN
   *
   * If a search string is given, do a non-interactive
   * search.
   */
  else {
    MWContext *   context_to_find;
    XFE_View *    view;
    fe_FindData * find_data;
    int           size;
    XP_Bool       b_allocated_locally = FALSE;

    /*
     * Get the appropriate context for the search.
     * This mimics what would be done in the function call stack
     * for frame->doCommand(xfeCmdFindAgain).
     */
    context_to_find = fe_findcommand_context();
    if (!context_to_find) {
      view = frame->getView();
      context_to_find = view->getContext();
    }

    /* ensure that an fe_FindData structure exists */
    find_data = CONTEXT_DATA(context_to_find)->find_data;
    if (!find_data) {
      /*
       * This section is modeled after fe_FindDialog() in dialogs.c
       */
      find_data = (fe_FindData *)XP_NEW_ZAP(fe_FindData); /* alloc and zero */
      b_allocated_locally = TRUE;
      CONTEXT_DATA(context_to_find)->find_data = find_data;

      find_data->context = context_to_find;
      find_data->find_in_headers = False;
      /* Q: as of this writing no support for mail/news headers? */

    }
    /* check that contexts point to eachother */
    XP_ASSERT(find_data->context == context_to_find);

    /*
     * Set the search parameters
     */
    if (find_data->string)
      XP_FREE(find_data->string);
    size = XP_STRLEN(search_string) + 1;
    find_data->string = (char*)XP_ALLOC(size);
    XP_STRCPY(find_data->string, search_string);
    find_data->case_sensitive_p  = b_is_case_sensitive;
    find_data->backward_p        = b_search_backwards;

    /*
     * This section modeled after fe_find() in dialogs.c
     */
    /* Q: what of ifdef EDITOR as in fe_find()? */
    LO_GetSelectionEndpoints(context_to_find,
			     &find_data->start_element,
			     &find_data->end_element,
			     &find_data->start_pos,
			     &find_data->end_pos,
                             &layer);

    result = LO_FindText(context_to_find, find_data->string,
			 &find_data->start_element, &find_data->start_pos, 
			 &find_data->end_element, &find_data->end_pos,
			 find_data->case_sensitive_p, !find_data->backward_p);
    if (result) {
      /*
       * This section is an exact cut and paste from fe_find().  Ugh!
       */
      int32 x, y;
      LO_SelectText (context_to_find,
		     find_data->start_element, find_data->start_pos,
		     find_data->end_element, find_data->end_pos,
		     &x, &y);

      /* If the found item is not visible on the screen, scroll to it.
	 If we need to scroll, attempt to position the destination
	 coordinate in the middle of the window.
	 */
      if (x >= CONTEXT_DATA (context_to_find)->document_x &&
	  x <= (CONTEXT_DATA (context_to_find)->document_x +
		CONTEXT_DATA (context_to_find)->scrolled_width))
	x = CONTEXT_DATA (context_to_find)->document_x;
      else
	x = x - (CONTEXT_DATA (context_to_find)->scrolled_width / 2);

      if (y >= CONTEXT_DATA (context_to_find)->document_y &&
	  y <= (CONTEXT_DATA (context_to_find)->document_y +
		CONTEXT_DATA (context_to_find)->scrolled_height))
	y = CONTEXT_DATA (context_to_find)->document_y;
      else
	y = y - (CONTEXT_DATA (context_to_find)->scrolled_height / 2);

      if (x + CONTEXT_DATA (context_to_find)->scrolled_width
	  > CONTEXT_DATA (context_to_find)->document_width)
	x = (CONTEXT_DATA (context_to_find)->document_width -
	     CONTEXT_DATA (context_to_find)->scrolled_width);

      if (y + CONTEXT_DATA (context_to_find)->scrolled_height
	  > CONTEXT_DATA (context_to_find)->document_height)
	y = (CONTEXT_DATA (context_to_find)->document_height -
	     CONTEXT_DATA (context_to_find)->scrolled_height);

      if (x < 0) x = 0;
      if (y < 0) y = 0;

      fe_ScrollTo (context_to_find, x, y);
    }

    /*
     * clean up after ourselves so that the widget text
     * field doesn't get confused during a subsequent
     * find dialog.
     */
    if (b_allocated_locally) {
      if (find_data->string)
	XP_FREE(find_data->string);
      XP_FREE(find_data);
      CONTEXT_DATA(context_to_find)->find_data = 0;
    }
  }
    
    

  return result;
}


extern "C" void
FE_GetPixelAndColorDepth(MWContext *pContext, /* input parameter */
			 int32 *pixelDepth, /* output parameter */
			 int32 *colorDepth) /* output parameter */
/*
 * description:
 *	Determine pixel depth and color depth.
 *	Pixel depth is the number of bits per pixel.
 *	Color depth is the number of color palette entries
 *	expressed as a power of two.  E.g. the color depth is 8
 *	for a palette with 256 colors.  In general the color depth
 *	is greater than or the same as the pixel depth.
 *
 * preconditions:
 *	All arguments must be non-NULL.
 *
 * returns:
 *	Return values are placed in pixelDepth and colorDepth
 *
 */
{
  XFE_DisplayFactory * factory;
  XFE_Frame *          frame;
  fe_colormap *        colormap;

  if (!pContext || !pixelDepth || !colorDepth)
    return;

  factory = XFE_DisplayFactory::theFactory();
  frame   = ViewGlue_getFrame(XP_GetNonGridContext(pContext));
  if (!factory || !frame)
    return;

  colormap = frame->getColormap();
  if (!colormap)
    return;

  *pixelDepth = factory->getVisualDepth();
  *colorDepth = fe_ColorDepth(colormap);
}


Boolean
fe_IsPageLoaded (MWContext *context)
{
  int i = 1;
  MWContext * child;
  
  if (context == NULL)
    return FALSE;
  
  if (NET_AreThereActiveConnectionsForWindow(context))
    return FALSE;
  
  if (context->mocha_context && JS_IsRunning(context->mocha_context))
    return FALSE;
  
  while ((child = (MWContext*)XP_ListGetObjectNum (context->grid_children,
						   i++)))
    if (!fe_IsPageLoaded(child))
      return FALSE;
  
  return TRUE;
}

MWContext *
xfe2_MakeNewWindow(Widget toplevel, MWContext *context_to_copy,
				   URL_Struct *url, char *window_name, MWContextType type,
				   Boolean /* skip_get_url */, Chrome *decor)
{
	XFE_Frame *parent_frame = context_to_copy ? ViewGlue_getFrame(context_to_copy) : 0;
	MWContext *new_context = 0;

	switch (type)
		{
		case MWContextSaveToDisk:
			{
				new_context = fe_showDownloadWindow(toplevel, parent_frame);
				break;
			}
		case MWContextBrowser:
			{
				new_context = fe_showBrowser(toplevel, parent_frame, decor, url);
				break;
			}
		case MWContextDialog:
			{
				new_context = fe_showHTMLDialog(toplevel, parent_frame, decor);
				break;
			}
#ifdef EDITOR
		case MWContextEditor:
			{
				new_context = fe_showEditor(toplevel, parent_frame, decor, url);
				break;
			}
#endif
#ifdef MOZ_MAIL_NEWS
		case MWContextMail:
		case MWContextNews: // shouldn't happen anymore...
			{
				MSG_PaneType paneType = url ? MSG_PaneTypeForURL(url->address) : MSG_FOLDERPANE;

				switch (paneType)
					{
					case MSG_FOLDERPANE:
						{
							new_context = fe_showFolders(toplevel, parent_frame, decor);
							break;
						}
					case MSG_THREADPANE:
						{
							MSG_FolderInfo *folder = MSG_GetFolderInfoFromURL(fe_getMNMaster(), url->address);
							if (folder)
								new_context = fe_showMessages(toplevel, parent_frame, decor, 
													   folder, fe_globalPrefs.reuse_thread_window, FALSE, MSG_MESSAGEKEYNONE);
							break;
						}
					case MSG_MESSAGEPANE:
						{
							MSG_FolderInfo *folder = MSG_GetFolderInfoFromURL(fe_getMNMaster(), url->address);
							MSG_MessageLine msgLine;
							MessageKey key = MSG_MESSAGEKEYNONE;

							if (MSG_GetMessageLineForURL(fe_getMNMaster(), url->address, &msgLine) >= 0)
								key = msgLine.messageKey;

							if (folder != NULL
								&& key != MSG_MESSAGEKEYNONE)
								{
									new_context = fe_showMsg(toplevel, parent_frame, decor, 
															 folder, key,
															 FALSE);
								}
							else
								{
									/* let's try and be generous here, since we really *should* pop up a window
									   in this case.*/
									XFE_MsgFrame *frame = new XFE_MsgFrame(toplevel, parent_frame, decor);

									frame->show();

									new_context = frame->getContext();

									FE_GetURL(new_context, url);
								}
							break;
						}
					default:
					  // XP_ASSERT(0);
#ifdef DEBUG_spence					  
					        printf ("unknown paneType\n");
#endif
						break;
					}
			}
			break;
		case MWContextSearch:
			{
#ifdef DEBUG_dora
				printf("[Warning]Want to create a search in fe_MakeNewWidget...\n");
#endif
				/* This should not be called here anymore in the new xfe2 design
				   We may consider remove this line all together....
				   Do we allow remote stuff to kick off the search without
				   Mail even being invoked?! If not, definitely remove this part here
				   */
				new_context = fe_showSearch(toplevel, parent_frame, decor);
				break;
			}
		case MWContextSearchLdap:
			{
#ifdef DEBUG_dora
				printf("[Warning]Want to create a ldap search in fe_MakeNewWidget...\n");
#endif
				/* This should not be called here anymore in the new xfe2 design
				   We may consider remove this line all together....
				   Do we allow remote stuff to kick off the search without
				   Mail even being invoked?! If not, definitely remove this part here
				   */
				new_context = fe_showLdapSearch(toplevel, parent_frame, decor);
				break;
			}
		case MWContextMessageComposition:
			{
				MSG_Pane *pane;
#ifdef DEBUG_dora
				printf("Calling fe_MakeNewWindow for MWContextMessageComposition\n" );
#endif
				pane = fe_showCompose(toplevel, decor, context_to_copy, NULL, NULL, False, False);
				if (pane) 
					new_context = MSG_GetContext(pane);
				break;
			}
		case MWContextAddressBook:
			{
				new_context = fe_showAddrBook (toplevel, NULL, decor);
				break;
			}
#endif  // MOZ_MAIL_NEWS
		default:
			XP_ASSERT(0);
			break;
		}

	// setup the stderr dialog stuff
	fe_initialize_stderr ();

	if (new_context && window_name)
	  new_context->name = strdup (window_name);
	return new_context;
}


/****************************************
 */

XP_Bool FE_IsNetcasterInstalled(void)

/*
 * description:
 *
 *
 * returns:
 *	TRUE or FALSE
 *
 ****************************************/
{
  return fe_IsNetcasterInstalled();
}


/****************************************
 */

void FE_RunNetcaster(MWContext * /* context */)

/*
 * description:
 *	The implementation is expected to
 *	be platform-specific, therefore the
 *	code goes here.
 *
 ****************************************/
{
  fe_showNetcaster(FE_GetToplevelWidget());
}

#ifdef IRIX
/****************************************
 *
 * SGI-specific debugging hack to workaround broken
 * interaction between NSPR and dbx/cvd.
 *
 * When you hit a breakpoint in dbx, type:
 *     "ccall fe_sgiStop()"
 *
 * dbx step and next commands will now work as expected
 * instead of hanging or dumping you in sigtramp()
 *
 * After you finish debugging, return to normal thread
 * operation by typing:
 *      "ccall fe_sgiStart()
 *
 * (It's not clear what will go wrong if you fail to restart
 * SIGALRM processing.)
 *****************************************/
#include <signal.h>

extern "C" void fe_sgiStart()
{
    sigset_t t;
    sigaddset(&t,SIGALRM);    
    sigprocmask(SIG_UNBLOCK,&t,NULL);
    
}

extern "C" void fe_sgiStop()
{
    sigset_t t;
    sigaddset(&t,SIGALRM);
    sigprocmask(SIG_BLOCK,&t,NULL);
}

#endif


//////////////////////////////////////////////////////////////////////////
//
// Progress functions.  Used to live in xfe/thermo.c
//
//////////////////////////////////////////////////////////////////////////

static void fe_frameNotifyLogoStopAnimation		(MWContext *);
static void fe_frameNotifyLogoStartAnimation	(MWContext *);

static void fe_frameNotifyProgressUpdateText	(MWContext *,char *);
static void fe_frameNotifyProgressUpdatePercent	(MWContext *,int);

static void	fe_frameNotifyProgressTickCylon		(MWContext *);

static void fe_frameNotifyStatusUpdateText		(MWContext *,char *);

//////////////////////////////////////////////////////////////////////////
static void
fe_frameNotifyLogoStartAnimation(MWContext * context)
{
	XP_ASSERT( context != NULL );

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	CONTEXT_DATA(context)->logo_animation_running = True;

	// Try to get the xfe frame for the context
	XFE_Frame * frame = fe_frameFromMWContext(context);
	
	// There will be no frame for biff context so we ignore them
	if (!frame)
	{
		return;
	}

	// Notify the frame to start animating the logo
	frame->notifyInterested(XFE_Frame::logoStartAnimation,(void *) NULL);
}
//////////////////////////////////////////////////////////////////////////
static void
fe_frameNotifyLogoStopAnimation(MWContext * context)
{
	XP_ASSERT( context != NULL );

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	CONTEXT_DATA(context)->logo_animation_running = False;

	// Try to get the xfe frame for the context
	XFE_Frame * frame = fe_frameFromMWContext(context);
	
	// There will be no frame for biff context so we ignore them
	if (!frame)
	{
		return;
	}

	// Notify the frame to stop animating the logo
	frame->notifyInterested(XFE_Frame::logoStopAnimation,(void *) NULL);
}
//////////////////////////////////////////////////////////////////////////
static void
fe_frameNotifyProgressUpdateText(MWContext * context,char * text)
{
	XP_ASSERT( context != NULL );

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	// Try to get the xfe frame for the context
	XFE_Frame * frame = fe_frameFromMWContext(context);
	
	// There will be no frame for biff context so we ignore them
	if (!frame)
	{
		return;
	}

	// Notify the frame to update the progress text
	frame->notifyInterested(XFE_Frame::progressBarUpdateText,(void *) text);
}
//////////////////////////////////////////////////////////////////////////
static void
fe_frameNotifyProgressUpdatePercent(MWContext * context,int percent)
{
	XP_ASSERT( context != NULL );

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	// Try to get the xfe frame for the context
	XFE_Frame * frame = fe_frameFromMWContext(context);
	
	// There will be no frame for biff context so we ignore them
	if (!frame)
	{
		return;
	}

	// Notify the frame to update the progress percent
	frame->notifyInterested(XFE_Frame::progressBarUpdatePercent,
							(void *) percent);
}

//////////////////////////////////////////////////////////////////////////
static void
fe_frameNotifyProgressTickCylon(MWContext * context)
{
	XP_ASSERT( context != NULL );

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	// Try to get the xfe frame for the context
	XFE_Frame * frame = fe_frameFromMWContext(context);
	
	// There will be no frame for biff context so we ignore them
	if (!frame)
	{
		return;
	}

	// Notify the frame to tick cylon mode
	frame->notifyInterested(XFE_Frame::progressBarCylonTick);
}
//////////////////////////////////////////////////////////////////////////
static void
fe_frameNotifyStatusUpdateText(MWContext * context,char * text)
{
	XP_ASSERT( context != NULL );

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	// Try to get the xfe frame for the context
	XFE_Frame * frame = fe_frameFromMWContext(context);
	
	// There will be no frame for biff context so we ignore them
	if (!frame)
	{
		return;
	}

	char * message = text;

    if (message == 0 || *message == '\0')
    {
		message = context->defaultStatus;
    }
    
	// Notify the frame to update the status text
    frame->notifyInterested(XFE_View::statusNeedsUpdating,(void*) message);
}
//////////////////////////////////////////////////////////////////////////


/* Print a status message in the wholine.
   A pointer to the string is not retained.
   */
//////////////////////////////////////////////////////////////////////////
extern "C" void
XFE_Progress(MWContext * context, const char * message)
{

#ifdef DEBUG_PROGRESS
	printf("XFE_Progress(%s)\n",message ? message : "BLANK");
#endif

	fe_frameNotifyStatusUpdateText(context,(char *) message);
}
//////////////////////////////////////////////////////////////////////////


/* This is so that we (attempt) to update the display at least once a second,
   even if the network library isn't calling us (as when the remote host is
   being slow.) 
   */
static void
fe_progress_dialog_timer (XtPointer closure, XtIntervalId * /* id */)
{
  MWContext *context = (MWContext *) closure;

  if (!CHECK_CONTEXT_AND_DATA(context))
  {
	  return;
  }

  fe_UpdateGraph (context, True);
  /* If we don't know the total length (meaning cylon-mode) update every
     1/10th second to make the slider buzz along.  Otherwise, if we know
     the length, only update every 1/2 second.
   */
  CONTEXT_DATA (context)->thermo_timer_id =
	  XtAppAddTimeOut (fe_XtAppContext,
					   (CONTEXT_DATA (context)->thermo_lo_percent <= 0
						? 100
						: 500),
					   fe_progress_dialog_timer, closure);
}

/* Start blinking the light and drawing the thermometer.
   This is done before the first call to FE_GraphProgressInit()
   to make sure that we indicate that we are busy before the
   first connection has been established.
 */
extern "C" void
fe_StartProgressGraph(MWContext * context)
{
#ifdef DEBUG_PROGRESS
	printf("fe_StartProgressGraph()\n");
#endif

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }
	
	fe_frameNotifyLogoStartAnimation(context);

	time_t now = time ((time_t *) 0);

	CONTEXT_DATA (context)->thermo_start_time = now;
	CONTEXT_DATA (context)->thermo_last_update_time = now;
	CONTEXT_DATA (context)->thermo_data_start_time = 0;

	CONTEXT_DATA (context)->thermo_size_unknown_count = 0;
	CONTEXT_DATA (context)->thermo_total = 0;
	CONTEXT_DATA (context)->thermo_current = 0;
	CONTEXT_DATA (context)->thermo_lo_percent = 0;

	if (!CONTEXT_DATA (context)->thermo_timer_id)
	{
		fe_progress_dialog_timer ((XtPointer)context, 0);
	}
}

/* Shut off the progress graph and blinking light completely.
 */
extern "C" void
fe_StopProgressGraph(MWContext * context_in)
{
#ifdef DEBUG_PROGRESS
	printf("fe_StopProgressGraph()\n");
#endif

	MWContext * context = XP_GetNonGridContext(context_in);

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	if (context == NULL) return;

	// Stop animating the logo
	fe_frameNotifyLogoStopAnimation(context);

	// Clear the progress bar
	fe_frameNotifyProgressUpdatePercent(context,0);
	fe_frameNotifyProgressUpdateText(context,"");

	if (CONTEXT_DATA (context)->thermo_timer_id)
    {
		XtRemoveTimeOut (CONTEXT_DATA (context)->thermo_timer_id);

		CONTEXT_DATA (context)->thermo_timer_id = 0;
    }

	/* Kludge to go out of "cylon mode" when we actually reach the end. */
	if (CONTEXT_DATA (context)->thermo_size_unknown_count > 0)
    {
		int size_guess = (CONTEXT_DATA (context)->thermo_total >
						  CONTEXT_DATA (context)->thermo_current
						  ? CONTEXT_DATA (context)->thermo_total
						  : CONTEXT_DATA (context)->thermo_current);
		CONTEXT_DATA (context)->thermo_size_unknown_count = 0;
		CONTEXT_DATA (context)->thermo_total = size_guess;
		CONTEXT_DATA (context)->thermo_current = size_guess;
    }
	/* Get the 100% message. */
	fe_UpdateGraph (context, True);

	/* Now clear the thermometer while not disturbing the text. */
	CONTEXT_DATA (context)->thermo_lo_percent = 0;

	fe_UpdateGraph (context, False);
}


/* Redraw the graph.  If text_too_p, then regenerate the textual message
   and/or move the cylon one tick if appropriate.  (We don't want to do
   this every time FE_GraphProgress() is called, just once a second or so.)
 */
extern "C" void 
fe_UpdateGraph(MWContext * context,Boolean text_too_p)
{
	double ratio_done;
	time_t now;
	int total_bytes, bytes_received;
	Boolean size_known_p;

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	now = time ((time_t *) 0);
	total_bytes     = CONTEXT_DATA (context)->thermo_total;
	bytes_received  = CONTEXT_DATA (context)->thermo_current;
	size_known_p    = CONTEXT_DATA (context)->thermo_size_unknown_count <= 0;

	if (size_known_p && total_bytes > 0 && bytes_received > total_bytes)
    {
#if 0
		/* Netlib doesn't take into account the size of the headers, so this
		   can actually go a bit over 100% (not by much, though.)  Prevent
		   the user from seeing this bug...   But DO print a warning if we're
		   way, way over the limit - more than 1k probably means the server
		   is messed up. */
		if (bytes_received > total_bytes + 1024)
			fprintf (stderr, "%s: received %d bytes but only expected %d??\n",
					 fe_progname, bytes_received, total_bytes);
#endif
		bytes_received = total_bytes;
    }

	ratio_done = (size_known_p && total_bytes > 0
				  ? (((double) bytes_received) / ((double) total_bytes))
				  : 0);

#ifdef DEBUG_PROGRESS
	printf(" fe_UpdateGraph: %d/%d=%f (%d/%d) %s  -or-  %d%%\n",
		   bytes_received, total_bytes,
		   ratio_done,
		   CONTEXT_DATA (context)->thermo_size_unknown_count,
		   CONTEXT_DATA (context)->active_url_count,
		   text_too_p ? "text" : "notext",
		   CONTEXT_DATA (context)->thermo_lo_percent);
#endif

	/* Update the thermo each time we're called. */

	if (CONTEXT_DATA (context)->thermo_lo_percent >= 0)
		{
			int percent = CONTEXT_DATA(context)->thermo_lo_percent;

			// Update the progress percent
			fe_frameNotifyProgressUpdatePercent(context,percent);
		}
		else
		{
			fe_frameNotifyProgressTickCylon(context);
		}

	/* Only update text if a second or more has elapsed since last time.
     Unlike the cylon, which we update each time we are called with
     text_too_p == True (which is 4x a second or so.)
   */
	if (text_too_p && now >= (CONTEXT_DATA (context)->thermo_last_update_time +
							  CONTEXT_DATA (context)->progress_interval))
    {
		const char *msg = XP_ProgressText ((size_known_p ? total_bytes : 0),
										   bytes_received, 
										   CONTEXT_DATA (context)->thermo_start_time,
										   now);

		CONTEXT_DATA (context)->thermo_last_update_time = now;

		if (msg && *msg)
			XFE_Progress (context, msg);

#ifdef DEBUG_PROGRESS
		fprintf (stderr, "====== %s\n", (msg ? msg : ""));
#endif
    }
}


/* Inform the user that a document is being transferred.
   URL is the url to which this report relates;
   bytes_received is how many bytes have been read;
   content_length is how large this document is (0 if unknown.)
   This is called from netlib as soon as we know the content length
    (or know that we don't know.)  It is called only once per document.
 */
extern "C" void
XFE_GraphProgressInit(MWContext *	context_in,
					  URL_Struct *	/* url_struct */,
					  int32			content_length)
{
#ifdef DEBUG_PROGRESS
	printf("XFE_GraphProgressInit(%ld)\n",content_length);
#endif

	MWContext * context = XP_GetNonGridContext(context_in);

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	if (!CONTEXT_DATA(context)->thermo_timer_id)
	{
		/* Hey!  How did that get turned off?  Turn it back on. */
		fe_StartProgressGraph (context);
	}
	
	if (content_length == 0)
	{
		CONTEXT_DATA(context)->thermo_size_unknown_count++;
	}
	else
	{
		CONTEXT_DATA(context)->thermo_total += content_length;
	}
}

/* Remove --one-- transfer from the progress graph.
 */
extern "C" void
XFE_GraphProgressDestroy(MWContext *	context_in,
						 URL_Struct *	/* url */,
						 int32			content_length,
						 int32			total_bytes_read)
{
#ifdef DEBUG_PROGRESS
	printf("XFE_GraphProgressDestroy(%ld, %ld)\n",
		   content_length, 
		   total_bytes_read);
#endif
	
	MWContext * context = XP_GetNonGridContext(context_in);
	
    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }
	
	if (content_length == 0)
    {
		/* Now that this transfer is done, we finally know how big it was.
		   This means that maybe we can go out of cylon mode and back into
		   thermometer mode. */
		CONTEXT_DATA (context)->thermo_size_unknown_count--;
		CONTEXT_DATA (context)->thermo_total += total_bytes_read;
    }
}


/* Inform the user of current progress, somehow.
   URL is the url to which this report relates;
   bytes_received is how many bytes have been read;
   content_length is how large this document is (0 if unknown.)
   This is called from netlib, and may be called very frequently.
 */
extern "C" void
XFE_GraphProgress(MWContext *	context_in,
				  URL_Struct *	/* url_struct */,
				  int32			/* bytes_received */,
				  int32			bytes_since_last_time,
				  int32			/* content_length */)
{
#ifdef DEBUG_PROGRESS
	printf("XFE_GraphProgress(%ld, +%ld, %ld)\n",
		   bytes_received, 
		   bytes_since_last_time, 
		   content_length);
#endif
	
	MWContext * context = XP_GetNonGridContext(context_in);

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

	if (CONTEXT_DATA (context)->thermo_data_start_time <= 0)
		/* This is the first chunk of bits to arrive. */
		CONTEXT_DATA (context)->thermo_data_start_time = time ((time_t *) 0);
	
	CONTEXT_DATA (context)->thermo_current += bytes_since_last_time;

	fe_UpdateGraph (context, False);
}

//////////////////////////////////////////////////////////////////////////
extern "C" void
XFE_SetProgressBarPercent(MWContext * context_in,int32 percent)
{
#ifdef DEBUG_PROGRESS
	printf("XFE_SetProgressBarPercent(%d)\n",percent);
#endif

	MWContext * context = XP_GetNonGridContext(context_in);

    if (!CHECK_CONTEXT_AND_DATA(context))
    {
		return;
    }

#ifndef LOU_OR_ERIC_REMOVED_EXPLICIT_CALL_TO_XFE_SETPROGRESSBAR_FROM_LAYOUT
	// Called during layout for print, do nothing
	if (CONTEXT_DATA(context) == NULL)
	{
		return;
	}
#endif

#ifdef EDITOR
	if (context->is_editor) 
	{
		/* the editor tells us 100% when it means 0 */
		if (percent == 100)
		{
			percent = 0;
		}
	}
#endif /* EDITOR */

	// Update the progress percent
	fe_frameNotifyProgressUpdatePercent(context,percent);

	// Clear the progress bar text for percent < 0
	if (percent < 0)
	{
		fe_frameNotifyProgressUpdateText(context,"");
	}
	else
	{
		if (percent > 100)
		{
			percent = 100;
		}

		char text[32];
		
		XP_SPRINTF(text,"%d%%",percent);
		
		// Update the progress text
		fe_frameNotifyProgressUpdateText(context,text);
	}

	CONTEXT_DATA(context)->thermo_lo_percent = percent;

	fe_UpdateGraph (context, True);
//	fe_UpdateGraph (context, False);
}
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
//
//   fe_NeutralizeFocus()
//
//   Move focus out of "Location:" and back to top-level.
//   Unfortunately, just focusing on the top-level doesn't do it - that
//   assigns focus to its first child, which may be the URL text field,
//   which is exactly what we're trying to avoid.  So, first try assigning
//   focus to a few other components if they exist.  Only try to assign focus
//   to the top level as a last resort.
//
//////////////////////////////////////////////////////////////////////////
extern "C" void
fe_NeutralizeFocus(MWContext * context)
{
	MWContext * top_context = XP_GetNonGridContext(context);

	XP_ASSERT( top_context != NULL );

	// Try to get the xfe frame for the context
	XFE_Frame * frame = fe_frameFromMWContext(top_context);

	// There will be no frame for biff context so we ignore them
	if (!frame)
	{
		return;
	}

	// Try the logo
	XFE_Logo * logo = frame->getLogo();

	if (logo && logo->processTraversal(XmTRAVERSE_CURRENT))
	{
		return;
	}

	// Try the dash board
	XFE_Dashboard * dash = frame->getDashboard();

	if (dash && dash->processTraversal(XmTRAVERSE_CURRENT))
	{
		return;
	}

	// Finally, try the top level widget
    XmProcessTraversal(CONTEXT_WIDGET(top_context),XmTRAVERSE_CURRENT);
}
//////////////////////////////////////////////////////////////////////////
extern "C" MWContext *FE_GetRDFContext(void) {
    MWContext *context = NULL;
    
/*    if(theApp.m_pRDFCX) {
        pRetval = theApp.m_pRDFCX->GetContext();
    }
*/    
    return(fe_getBookmarkContext());
}

/* this is here so that we can get access to frame/component utilities
   like fe_frameFromMWContext()...
   */

#ifdef SHACK

extern "C" void
fe_showRDFView (Widget parent, int width, int height)
{
#ifdef DEBUG_spence
  printf ("fe_showRDFView\n");
#endif

#if 0
  XtVaSetValues (parent, XmNwidth, width, XmNheight, height, 0);
  XtRealizeWidget (parent);
#endif

  MWContext *context = fe_WidgetToMWContext (parent);
  XFE_Component *toplevel = fe_frameFromMWContext (context);
  XFE_View *view = new XFE_NavCenterView (toplevel, parent, NULL, context);

  if (view == NULL) {
#ifdef DEBUG_spence
    printf ("fe_showRDFView: view creation failed\n");
#endif
  }

#ifdef DEBUG_spence
  printf ("fe_showRDFView: width %d: height %d\n", width, height);
#endif

  XtVaSetValues (view->getBaseWidget(), XmNwidth, width, XmNheight, height, 0);
  XtRealizeWidget (parent);
}

#endif /* SHACK */
