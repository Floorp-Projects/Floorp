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
   MozillaApp.cpp -- class definition for Mozilla-global stuff
   Created: Spence Murray <spence@netscape.com>, 12-Nov-96.
 */



#include "Frame.h"
#include "MozillaApp.h"
#include "ViewGlue.h"
#include "Dashboard.h"

#ifdef MOZ_MAIL_NEWS
#include "MNView.h"
#endif

#include <np.h>
#include "secnav.h"
#ifdef JAVA
#include "mozjava.h"
#endif /* JAVA */

#include "libmocha.h"
#include "libevent.h"

#include "BookmarkFrame.h" /* Used for FE_GetNetHelpContext */
#include "prefs.h"

#ifdef DEBUG_username
#define D(x) x
#else
#define D(x)
#endif

#define STANDARD_INSTALL_PATH "/usr/local/lib/netscape/"

extern "C" { 
	void xfe_TextCopy  (MWContext*, Widget, XP_Bool);
	void xfe_TextPaste (MWContext*, Widget, XP_Bool);
}

/* Keep track how many windows are about to be closed upon exit */
extern "C" void fe_GetProgramDirectory(char *path, int len); /* Used by FE_GetNetHelpDir */
#ifndef NO_WEB_FONTS
extern "C" void fe_ShutdownWebfonts(void);	/* fonts.c : Webfonts */
#endif /* NO_WEB_FONTS */

static XFE_MozillaApp *_app = NULL;

const char *XFE_MozillaApp::appBusyCallback = "XFE_MozillaApp::appBusyCallback";
const char *XFE_MozillaApp::appNotBusyCallback = "XFE_MozillaApp::appNotBusyCallback";
const char *XFE_MozillaApp::changeInToplevelFrames = "XFE_MozillaApp::changeInToplevelFrames";
const char *XFE_MozillaApp::bookmarksHaveChanged = "XFE_MozillaApp::bookmarksHaveChanged";
const char *XFE_MozillaApp::biffStateChanged = "XFE_MozillaApp::biffStateChanged";
const char *XFE_MozillaApp::updateToolbarAppearance = "XFE_MozillaApp::updateToolbarAppearance";
const char *XFE_MozillaApp::linksAttributeChanged = "XFE_MozillaApp::linksAttributeChanged";
const char *XFE_MozillaApp::defaultColorsChanged = "XFE_MozillaApp::defaultColorsChanged";
const char *XFE_MozillaApp::defaultFontChanged = "XFE_MozillaApp::defaultFontChanged";
const char *XFE_MozillaApp::refreshMsgWindow = "XFE_MozillaApp::refreshMsgWindow";
const char *XFE_MozillaApp::personalToolbarFolderChanged = "XFE_MozillaApp::personalToolbarFolderChanged";

static void xfeDoCommandAction(Widget w, XEvent *, String *, Cardinal *);
static void xfeDoTextAction(Widget w, XEvent *, String *, Cardinal *);
static void xfeDoPopupAction(Widget w, XEvent *, String *, Cardinal *);
static void xfeDoClickAction(Widget w, XEvent *, String *, Cardinal *);
static void xfeUndefinedKeyAction(Widget w, XEvent *, String *, Cardinal *);

XFE_MozillaApp::XFE_MozillaApp(int */*argc*/, char **/*argv*/)
{
  XFE_MozillaApp();
}

// hacked constructor.  should go away soon...
XFE_MozillaApp::XFE_MozillaApp()
{
  // only allow one XFE_MozillaApp to be created.
  XP_ASSERT(_app == NULL);

  m_frameList = XP_ListNew();
  m_editorFrameList = XP_ListNew();
  m_composeFrameList = XP_ListNew();
  m_msgFrameList = XP_ListNew();
  m_threadFrameList = XP_ListNew();
  m_folderFrameList = XP_ListNew();
  m_browserFrameList = XP_ListNew();
  m_addressbookFrameList = XP_ListNew();
  m_bookmarkFrameList = XP_ListNew();
  m_mailfilterFrameList = XP_ListNew();
  m_searchFrameList = XP_ListNew();
  m_ldapFrameList = XP_ListNew();
  m_maildownloadFrameList = XP_ListNew();
  m_historyFrameList = XP_ListNew();
  m_downloadFrameList = XP_ListNew();
  m_htmldialogFrameList = XP_ListNew();
  m_navcenterFrameList = XP_ListNew();

  m_isbusy = FALSE;
  m_exiting = FALSE;
  m_exitstatus = 0;
  m_actioninstalled = FALSE;
  m_exitwindowcount = 0;

  fe_MNWindowCount = 0;
  session_frame = NULL;

  _app = this;

  theApp()->registerInterest(XFE_MozillaApp::appBusyCallback,
							 this,
							 (XFE_FunctionNotification)updateBusyState_cb,
							 (void*)True);
  theApp()->registerInterest(XFE_MozillaApp::appNotBusyCallback,
							 this,
							 (XFE_FunctionNotification)updateBusyState_cb,
							 (void*)False);
}

/* recursively close all frames because frames might be removed from
   the list inside "doClose()". Therefore, the head of the frame list
   is easily moved around */
static void
next_and_close(XP_List* list_entry)
{
	XFE_Frame* frame = (XFE_Frame*)list_entry->object;
	XP_List*   next = list_entry->next;

	if (next != NULL)
		next_and_close(next);

	if (frame != NULL)
		frame->doClose();
}

void
XFE_MozillaApp::closeFrames(XP_List *frame_list)
{
	if (frame_list && frame_list->next)
		next_and_close(frame_list->next);
}

XP_Bool
XFE_MozillaApp::isOkToExitFrameList(XP_List *frame_list)
{
        XP_List *start;
	XP_Bool okFlag = True;
        XFE_Frame *frame;

        start = frame_list;

        while ((frame = (XFE_Frame*)XP_ListNextObject(start)) != NULL)
        {
			XP_ASSERT(frame);
			if (frame && ( fe_IsContextProtected(frame->getContext()) ||
				!(okFlag = frame->isOkToClose())  ) ) 
			{ 
			  okFlag = False;
			  break;
			}
		}
	return okFlag;	
}

XP_Bool
XFE_MozillaApp::isOkToExitMailNewsFrames()
{
  XP_Bool okFlag = True;
	if (isOkToExitFrameList(m_msgFrameList) ) 
	{
	   closeFrames(m_msgFrameList);
	   if ( isOkToExitFrameList(m_threadFrameList) )
	   {
	   	closeFrames(m_threadFrameList);
	    if ( isOkToExitFrameList(m_folderFrameList) )
		{
	   	    closeFrames(m_folderFrameList);
		}
		else okFlag = False;
	   }
	   else okFlag = False;
	}
	else okFlag = False;

  return okFlag;
}

XP_Bool
XFE_MozillaApp::isOkToExitNonMailNewsFrames()
{
  if ( isOkToExitFrameList(m_bookmarkFrameList) &&
     isOkToExitFrameList(m_editorFrameList) &&
     isOkToExitFrameList(m_composeFrameList) &&
     isOkToExitFrameList(m_searchFrameList) &&
     isOkToExitFrameList(m_ldapFrameList) &&
     isOkToExitFrameList(m_addressbookFrameList) &&
     isOkToExitFrameList(m_mailfilterFrameList) &&
     isOkToExitFrameList(m_maildownloadFrameList) &&
     isOkToExitFrameList(m_historyFrameList) &&
     isOkToExitFrameList(m_downloadFrameList) &&
     isOkToExitFrameList(m_htmldialogFrameList) &&
     isOkToExitFrameList(m_navcenterFrameList)
       )
     return True;

  return False;
}

void
XFE_MozillaApp::exit(int status)
{
	// if we end up here again somehow, don't free the stuff again.
	D(printf ("XFE_MozillaApp::exiting...%d\n", m_exiting);)

	if (m_exiting)
		{
			return;
		}

	/* Try to do these in decreasing order of importance, in case the user
	   gets impatient and kills us harder. */

	m_exiting = TRUE;

	/* Non-MailNews-Frame check */
	if (!isOkToExitNonMailNewsFrames())  
		{
			m_exiting = FALSE;
			return;
		}
	else 
		{
#ifdef MOZ_TASKBAR
			// Close the floating taskbar
			XFE_Dashboard::stopFloatingTaskBar();
#endif

			closeFrames(m_bookmarkFrameList);
			closeFrames(m_editorFrameList); 
			closeFrames(m_composeFrameList); 
			closeFrames(m_searchFrameList);
			closeFrames(m_ldapFrameList);
			closeFrames(m_addressbookFrameList);
			closeFrames(m_mailfilterFrameList);
			closeFrames(m_maildownloadFrameList);
			closeFrames(m_historyFrameList);
			closeFrames(m_downloadFrameList);
			closeFrames(m_htmldialogFrameList);
			closeFrames(m_browserFrameList);
			closeFrames(m_navcenterFrameList);
		}
	if (!isOkToExitMailNewsFrames() )
		{
			m_exiting = FALSE;
			return;
		}

	//
	//    At this point we are commited to exiting. We can't do everything
	//    yet, but we've started. We've do far XtDestroy()ed all Frames
	//    but Bookmarks. Once we get back to the event loop, Xt will start
	//    the second (and real) phase of destruction, and will call all
	//    the Component sub-class destructors in the process. Frame
	//    destructors call MozillaApp::unregisterFrame() below, and this
	//    counts down the number of frames. When there are only Bookmarks
	//    (or no) frames left, we do the final exit: MOZ_byebye().
	//
	//    Save the requested exit status here.
	//
	m_exitstatus = status;

	/* My theory for putting this first is that if the user's keys get
	 * trashed they are really hurt, and if their certs get trashed
	 * they may have to pay $$$ to get new ones.  If you disagree,
	 * lets talk about it.  --jsw
	 */
	SECNAV_Shutdown();

	GH_SaveGlobalHistory ();

	//
	//    Check now if we are done. If no Frame are up any more,
	//    there is nothing to callback from a destroy callback,
	//    so we'd just go back to fe_EventLoop() and spin forever.
	//    Let's not do that. Instead, just go now.
	//
	//    We should only go this path when the last thing out is
	//    a non-XFE_Frame - for example XFE_TaskBar.
	//
	if (timeToDie()) {
		byebye(m_exitstatus);
		/*NOTREACHED*/
	}
}

static XtActionsRec xfe_actions[] = {
	{ "xfeDoCommand", xfeDoCommandAction },
	{ "xfeDoText",    xfeDoTextAction    },
	{ "xfeDoPopup",   xfeDoPopupAction   },
	{ "xfeDoClick",   xfeDoClickAction   },
	{ "undefined-key", xfeUndefinedKeyAction },
};

void
XFE_MozillaApp::byebye(int status)
{
	D(printf("MOZ_byebye() entered, grim reaper doing his stuff.... die die die..\n");)

	//
	//    Shutdown bookmarks.
	//
	D(printf("Bookmark frame....die die die..\n");)
	XFE_Frame* b_frame = (XFE_Frame*)XP_ListNextObject(m_bookmarkFrameList);
	if (b_frame != NULL) {
		XtDestroyWidget(b_frame->getBaseWidget());
	}

#ifdef MOZ_MAIL_NEWS
	/* This only calls MSG_blah functions, so I'm assuming it isn't necessary for non MOZ_MAIL_NEWS. */
	XFE_MNView::destroyMasterAndShutdown();
#endif

	ET_FinishMocha ();

#ifdef JAVA
	LJ_ShutdownJava();
#endif /* JAVA */
	NET_CleanupCacheDirectory (FE_CacheDir, "cache");
	GH_SaveGlobalHistory ();
	GH_FreeGlobalHistory ();
	NET_ShutdownNetLib ();
	NPL_Shutdown ();
	HOT_FreeBookmarks ();
	GH_FreeGlobalHistory ();

	/* Mailcap and MimeType have been written out when the prefdialog was closed */
	/* We don't need to write again. Just, Clean up these cached mailcap */
	/* and mimetype link list */
	NET_CleanupFileFormat(NULL);
	NET_CleanupMailCapList(NULL);

	/* Cleanup webfonts */
	fe_ShutdownWebfonts();

	/* Save prefs so some pref strings can be saved when the user exits. e.g. print command
	 */

	XFE_SavePrefs ((char *) fe_globalData.user_prefs_file, &fe_globalPrefs);

	if (fe_pidlock) unlink (fe_pidlock);
	fe_pidlock = 0;
	::exit(status);

	/*NOTREACHED*/
}

XP_Bool
XFE_MozillaApp::timeToDie()
{
	//
	//    Test to see if the last non-singleton frame has been destroyed.
	//    If it has, then it's time to exit().
	//
	//    The only possible singleton frames are history and bookmarks.
	//
	int singleton_count = 
		XP_ListCount(m_bookmarkFrameList) + 
		XP_ListCount(m_historyFrameList);

	//
	// Cannot exit if the taskbar is not docked (ie, floating)
	//
	int taskbar_count;

#ifdef MOZ_TASKBAR
	taskbar_count = !XFE_Dashboard::isTaskBarDocked();
#else
	taskbar_count = 0;
#endif

	if ((m_exitwindowcount + taskbar_count) <= singleton_count)
		return TRUE;
	else
		return FALSE;
}

void
XFE_MozillaApp::unregisterFrame(XFE_Frame * /*f*/)
{
	m_exitwindowcount--;

	if (timeToDie()) {
		byebye(m_exitstatus);
		/*NOTREACHED*/
	}
}

void
XFE_MozillaApp::registerFrame(XFE_Frame *f)
{
  XP_ListAddObject(m_frameList, f);

  D(printf ("Registering frame : ");)
  switch (f->getType())
    {
    case FRAME_MAILNEWS_MSG:
      D(printf ("type=MailNewsMsg\n");)
      XP_ListAddObject(m_msgFrameList, f);
      fe_WindowCount++;
      fe_MNWindowCount++;
      break;
    case FRAME_MAILNEWS_THREAD:
      D(printf ("type=MailNewsThread\n");)
      XP_ListAddObject(m_threadFrameList, f);
      fe_WindowCount++;
      fe_MNWindowCount++;
      break;
    case FRAME_MAILNEWS_FOLDER:
      D(printf ("type=MailNewsFolder\n");)
      XP_ListAddObject(m_folderFrameList, f);
      fe_WindowCount++;
      fe_MNWindowCount++;
      break;
    case FRAME_MAILNEWS_COMPOSE:
      D(printf ("type=MailNewsCompose\n");)
      XP_ListAddObject(m_composeFrameList, f);
      fe_WindowCount++;
      break;
    case FRAME_MAILNEWS_SEARCH:
      D(printf ("type=MailNewsSearch\n");)
      XP_ListAddObject(m_searchFrameList, f);
      fe_WindowCount++;
      break;
    case FRAME_HTML_DIALOG:
      D(printf ("type=HtmlDialog\n");)
      XP_ListAddObject(m_htmldialogFrameList, f);
      fe_WindowCount++;
      break;
    case FRAME_MAILNEWS_DOWNLOAD:
      D(printf ("type=MailNewsDownload\n");)
      XP_ListAddObject(m_maildownloadFrameList, f);
      fe_WindowCount++;
      break;
    case FRAME_LDAP_SEARCH:
      D(printf ("type=LdapSearch\n");)
      XP_ListAddObject(m_ldapFrameList, f);
      fe_WindowCount++;
      break;
    case FRAME_EDITOR:
      D(printf ("type=Editor\n");)
      XP_ListAddObject(m_editorFrameList, f);
      fe_WindowCount++;
      break;
    case FRAME_BROWSER:
      D(printf ("type=Browser\n");)
      XP_ListAddObject(m_browserFrameList, f);
      fe_WindowCount++;
      break;
    case FRAME_ADDRESSBOOK:
      D(printf ("type=AddressBook\n");)
      XP_ListAddObject(m_addressbookFrameList, f);
      fe_WindowCount++;
      break;
    case FRAME_BOOKMARK:
      D(printf ("type=Bookmark\n");)
      XP_ListAddObject(m_bookmarkFrameList, f);
      break;
    case FRAME_MAILFILTER:
      D(printf ("type=MailFilter\n");)
      XP_ListAddObject(m_mailfilterFrameList, f);
      break;
    case FRAME_HISTORY:
      D(printf ("type=History\n");)
      XP_ListAddObject(m_historyFrameList, f);
      break;
	case FRAME_DOWNLOAD:
      D(printf ("type=Download\n");)
      XP_ListAddObject(m_downloadFrameList, f);
      fe_WindowCount++;
      break;
	case FRAME_NAVCENTER:
      D(printf ("type=NavCenter\n");)
      XP_ListAddObject(m_navcenterFrameList, f);
      fe_WindowCount++;
      break;
    default:
      XP_ASSERT(0);
      break;
    }

  f->registerInterest(XFE_Frame::beforeDestroyCallback,
		      this,
		      (XFE_FunctionNotification)frameUnregistering_cb);

  // set up the xfeDoCommand stuff if it hasn't already been installed.
  if (!m_actioninstalled)
    {
	  XtAppAddActions(fe_XtAppContext, xfe_actions, XtNumber(xfe_actions));

      m_actioninstalled = TRUE;
    }

  // now we let everyone know that there has been a change in the frames
  // so the Frames can update their close menu items, and the menubar can
  // regenerate the windows menu.
  notifyInterested(changeInToplevelFrames, (void*)fe_WindowCount);

  //
  // One more window to add to the number we must wait for when existing.
  //
  m_exitwindowcount++;
}

XP_Bool XFE_MozillaApp::getBusy()
{
	return m_isbusy;
}

XFE_CALLBACK_DEFN(XFE_MozillaApp, updateBusyState)(XFE_NotificationCenter */*obj*/,
												   void *clientData,
												   void */*callData*/)
{
	m_isbusy = (XP_Bool)(int)clientData;
}

XFE_CALLBACK_DEFN(XFE_MozillaApp, frameUnregistering)(XFE_NotificationCenter *obj,
						  void */*clientData*/,
						  void */*callData*/)
{
  XFE_Frame *f = (XFE_Frame*)obj;

  /* only worry about keeping the lists up-to-date if we're going to
     be sticking around. */
    {   
      D(printf ("Unregistering frame : ");)
	
	/* Unmap the window right away to give feedback that the delete
	   is in progress */
	Widget w = f->getBaseWidget();
	if (XtWindow(w))
	  XUnmapWindow(XtDisplay(w), XtWindow(w));

      fe_WindowCount --;

      XP_ListRemoveObject(m_frameList, f);
      
      switch (f->getType())
	{
	case FRAME_MAILNEWS_MSG:
	  D(printf ("type=MailNewsMsg\n");)
	  XP_ListRemoveObject(m_msgFrameList, f);
          fe_MNWindowCount--;
	  break;
	case FRAME_MAILNEWS_THREAD:
	  D(printf ("type=MailNewsThread\n");)
	  XP_ListRemoveObject(m_threadFrameList, f);
          fe_MNWindowCount--;
	  break;
	case FRAME_MAILNEWS_FOLDER:
	  D(printf ("type=MailNewsFolder\n");)
	  XP_ListRemoveObject(m_folderFrameList, f);
          fe_MNWindowCount--;
	  break;
	case FRAME_MAILNEWS_COMPOSE:
	  D(printf ("type=MailNewsCompose\n");)
	  XP_ListRemoveObject(m_composeFrameList, f);
	  break;
	case FRAME_MAILNEWS_SEARCH:
	  D(printf ("type=MailNewsSearch\n");)
	  XP_ListRemoveObject(m_searchFrameList, f);
	  break;
	case FRAME_MAILNEWS_DOWNLOAD:
	  D(printf ("type=MailNewsDownload\n");)
	  XP_ListRemoveObject(m_maildownloadFrameList, f);
	  break;
	case FRAME_LDAP_SEARCH:
	  D(printf ("type=LdapSearch\n");)
	  XP_ListRemoveObject(m_ldapFrameList, f);
	  break;
	case FRAME_EDITOR:
	  D(printf ("type=Editor\n");)
	  XP_ListRemoveObject(m_editorFrameList, f);
	  break;
	case FRAME_BROWSER:
	  D(printf ("type=Browser\n");)
	  XP_ListRemoveObject(m_browserFrameList, f);
	  break;
	case FRAME_ADDRESSBOOK:
	  D(printf ("type=AddressBook\n");)
	  XP_ListRemoveObject(m_addressbookFrameList, f);
	  break;
	case FRAME_BOOKMARK:
	  D(printf ("type=Bookmark\n");)
	  XP_ListRemoveObject(m_bookmarkFrameList, f);
	  break;
	case FRAME_MAILFILTER:
	  D(printf ("type=MailFilter\n");)
	  XP_ListRemoveObject(m_mailfilterFrameList, f);
	  break;
	case FRAME_HISTORY:
	  D(printf ("type=History\n");)
	  XP_ListRemoveObject(m_historyFrameList, f);
	  break;
	case FRAME_DOWNLOAD:
      D(printf ("type=Download\n");)
      XP_ListRemoveObject(m_downloadFrameList, f);
      break;
	case FRAME_HTML_DIALOG:
      D(printf ("type=HtmlDialog\n");)
      XP_ListRemoveObject(m_htmldialogFrameList, f);
      break;
	case FRAME_NAVCENTER:
      D(printf ("type=navcenter\n");)
      XP_ListRemoveObject(m_navcenterFrameList, f);
      break;
	}

      // Call exit() *after* removing the frame from the XP list
      // to avoid closing the same frame twice.
      if (fe_WindowCount == 0 && (!m_exiting))
	exit(0);

      // FIX ME : Do session manager stuff here if this frame was the one
      // with the session manager callback stuff being handled.

      // now we let everyone know that there has been a change in the frames
      // so the Frames can update their close menu items, and the menubar can
      // regenerate the windows menu.
      notifyInterested(changeInToplevelFrames, (void*)fe_WindowCount);
    }
}

XP_List *
XFE_MozillaApp::getAllFrameList()
{
	return m_frameList;
}


XP_List *
XFE_MozillaApp::getFrameList(EFrameType type)
{
  switch (type)
    {
    case FRAME_BROWSER:
      return m_browserFrameList;
    case FRAME_EDITOR:
      return m_editorFrameList;
    case FRAME_MAILNEWS_COMPOSE:
      return m_composeFrameList;
    case FRAME_MAILNEWS_SEARCH:
      return m_searchFrameList;
    case FRAME_LDAP_SEARCH:
      return m_ldapFrameList;
    case FRAME_MAILNEWS_MSG:
      return m_msgFrameList;
    case FRAME_MAILNEWS_THREAD:
      return m_threadFrameList;
    case FRAME_MAILNEWS_FOLDER:
      return m_folderFrameList;
    case FRAME_MAILNEWS_DOWNLOAD:
	  return m_maildownloadFrameList;
    case FRAME_MAILFILTER:
      return m_mailfilterFrameList;
    case FRAME_ADDRESSBOOK:
      return m_addressbookFrameList;
    case FRAME_BOOKMARK:
      return m_bookmarkFrameList;
    case FRAME_HISTORY:
      return m_historyFrameList;
    case FRAME_NAVCENTER:
      return m_navcenterFrameList;
    default:
      XP_ASSERT(0);
      return NULL;
    }
}


XFE_MozillaApp *
XFE_MozillaApp::theApp()
{
  if (!_app)
    {
      new XFE_MozillaApp(); // create _app
    }
  
  return _app;
}

int
XFE_MozillaApp::toplevelWindowCount()
{
  return fe_WindowCount;
}

int
XFE_MozillaApp::mailNewsWindowCount()
{
  return fe_MNWindowCount;
}

// C API.  so we can initialize it in main
XFE_MozillaApp *
theMozillaApp()
{
  return XFE_MozillaApp::theApp();
}

// The text cut/copy/paste action handler for the entire app...
//
static void
xfeDoTextAction(Widget w, XEvent *event,
				String *params, Cardinal *num_params)
{
	MWContext *context = fe_WidgetToMWContext(w);
	CommandType cmd;

	if (!context) {
#ifdef DEBUG_rhess
		fprintf(stderr, "xfeDoTextAction::[ no context ] **\n");
#endif
		return;
	}

	if (*num_params < 1) { 
#ifdef DEBUG_rhess
		fprintf(stderr, "xfeDoTextAction::[ no command ] **\n");
#endif
		// We must be sent the command to be performed.
		return;
	}
	
	cmd = Command::intern(params[0]);

	if (!XmIsText(w) && !XmIsTextField(w)) {
#ifdef DEBUG_rhess
		fprintf(stderr, "xfeDoTextAction::[ no widget ] **\n");
#endif
		// We must be sent the command to be performed.
		return;
	}

	if (cmd == xfeCmdCut) {
#ifdef DEBUG_rhess
		fprintf(stderr, "xfeDoTextAction::[ %s ]\n", cmd);
#endif
		xfe_TextCopy (context, w, TRUE);
	}
	else {
		if (cmd == xfeCmdCopy) {
#ifdef DEBUG_rhess
			fprintf(stderr, "xfeDoTextAction::[ %s ]\n", cmd);
#endif
			xfe_TextCopy (context, w, FALSE);
		}
		else {
			if (cmd == xfeCmdPaste) {
#ifdef DEBUG_rhess
				fprintf(stderr, "xfeDoTextAction::[ %s ]\n", cmd);
#endif
				xfe_TextPaste (context, w, FALSE);
			}
			else {
#ifdef DEBUG_rhess
				fprintf(stderr,
						"xfeDoTextAction::[ no handler ][ %s ] **\n", 
						cmd);
#endif
				XBell (XtDisplay(w), 0);
				return;
			}
		}
	}
}

// C API to our xfeDoCommandAction() function. We use this as our
// remote entry point.
extern "C" void
xfeDoRemoteCommand (Widget w, XEvent *event,
		    String *params, Cardinal *num_params)
{
  int first_param = 0;
  Cardinal numparams = *num_params;
  String old_first;

#ifdef DEBUG_spence
  printf ("xfeDoRemoteCommand: params[0] = %s\n", params[0]);
#endif

  /* if the first argument is xfeDoCommand, skip it. */
  if (!strcmp(params[0], "xfeDoCommand"))
	{
	  first_param = 1;
	  numparams --;
	}

  old_first = params[first_param];
  params[first_param] = Command::convertOldRemote(params[first_param]);

  xfeDoCommandAction (w, event, &params[first_param], &numparams);

  params[first_param] = old_first;
}

// The *one* action handler for the entire app.  All it does is figure out
// what frame it's suppose to use, verify that the command (sent as params[0])
// is handled and enabled, and then calls doCommand().  Neat, huh?
static void
xfeDoCommandAction(Widget w, XEvent *event,
		   String *params, Cardinal *num_params)
{
	MWContext *context = fe_WidgetToMWContext(w);
	CommandType cmd;
	XFE_Frame *f;
	
	if (*num_params < 1) // We must be sent the command to be performed.
		{
			D(  printf ("There must be at least 1 parameter.  There is %d\n", *num_params);)
				return;
		}
	
	f = ViewGlue_getFrame(XP_GetNonGridContext(context));
	
	if (!f) // hmm... don't know *how* we could have gotten here otherwise...
		return;
	
	cmd = Command::intern(params[0]);
#ifdef DEBUG_spence
	printf ("cmd: %s\n", cmd);
#endif
	
	/* xfeDoCommand(showPopup) and popup() both do the same thing */
	if (cmd == xfeCmdShowPopup
		&& // dealing with xfeDoPopupAction() is beyond me...djw
		XP_STRCMP(XtName(w), "editorDrawingArea") != 0)
		{
			xfeDoPopupAction(w, event, params, num_params);
			return;
		}
	// Further justification: I really want commands to be delivered to
	// the Frame. xfeDoPopupAction() delivers the command to the view.
	// This is different from every other command, and breaks the
	// way EditorFrame and EditorView talk to each other. So, for now,
	// I'm special casing this (I know it's hacky) for the Editor
	// and HTML mail compose...djw
	
	String* p_params = NULL;
	XFE_CommandEventType cmd_type;
	int num_cmd_params = *num_params;
	int i;

	if (*num_params > 1) // first is name of command.
	  p_params = &params[1];
	else
	  p_params = NULL;

	num_cmd_params -= 1;

	if (p_params && 
		p_params[num_cmd_params - 1] &&
		!strcmp(p_params[num_cmd_params - 1], "<remote>"))
	  {
		cmd_type = XFE_COMMAND_REMOTE_ACTION;
		num_cmd_params -= 1;
	  }
	else
	  cmd_type = XFE_COMMAND_EVENT_ACTION;

	if (num_cmd_params == 0)
		p_params = NULL;
	
	XFE_CommandInfo info(cmd_type, w, event,
						 p_params, num_cmd_params);

	/* if the frame doesn't handle the command, bomb out early */
	if (!f->handlesCommand(cmd, NULL, &info))
	  {
		XBell (XtDisplay(f->getBaseWidget()), 0);
		return;
	  }

	/* if hotkeys are disabled, beep and return */
	if (cmd_type != XFE_COMMAND_REMOTE_ACTION &&
		f->hotKeysDisabled() &&
		/*
		** This following set of commands is allowed through regardless of
		** whether or not hot keys are enabled
		*/
		cmd != xfeCmdExit &&
		cmd != xfeCmdViewSecurity &&
		cmd != xfeCmdCut &&
		cmd != xfeCmdCopy &&
		cmd != xfeCmdPaste)
	  {
		XBell (XtDisplay(f->getBaseWidget()), 0);
		return;
	  }

	if (f->isCommandEnabled(cmd, NULL, &info)) {
		
		xfe_ExecuteCommand(f, cmd, NULL, &info);
	}
}

static void
xfeDoPopupAction(Widget w,
		 XEvent *event,
		 String */*params*/, Cardinal */*num_params*/)
{
  CommandType cmd;
  XFE_Frame *f;

#ifdef MOZ_MAIL_NEWS
  XFE_View *v;
#endif

  MWContext *context = fe_WidgetToMWContext(w);

  f = ViewGlue_getFrame(XP_GetNonGridContext(context));

  if (!f) {
	  // hmm... don't know *how* we could have gotten here otherwise...
	  return;
  }/* if */

  cmd = xfeCmdShowPopup;

  XFE_CommandInfo info(XFE_COMMAND_EVENT_ACTION, w, event, NULL, 0);

#ifdef MOZ_MAIL_NEWS
  v = f->widgetToView(w);

  if (!v) {
#endif
	  if (f) {
		  if (f->handlesCommand(cmd))
			  f->doCommand(cmd, NULL, &info);
	  }/* if */
#ifdef MOZ_MAIL_NEWS
  }/* if */
  else if (v->handlesCommand(cmd))
	  v->doCommand(cmd, NULL, &info);
#endif
}

//
//    This action is used to diferentiate single/double clicks,
//    which Xt does a bad job on. In your translation use:
//
//    binding: xfeDoClick(single, { command1,p1,p2,p3,...,}, \
//                        double, { command2,p1,p2,p3,})
//
//    Note: the zero length string as the param before double.
//
//    xfeDoClick() will dispatch via xfeDoCommand(). If it's a single click,
//    xfeDoCommand(command1, p1, p2, ...) will be called. If it's a double,
//    xfeDoCommand(command2, p1, p2, ...) will be called. It doesn't handle
//    triple clicks.
//
static Time fe_click_action_last; /* this is very lazy */
static void
xfeDoClickAction(Widget widget, XEvent *event, String *av, Cardinal *ac)
{
    Time     time;
    unsigned n;
    unsigned m;
    unsigned n_clicks = 1;
    unsigned c_clicks = 1;
	String   command_name;
	unsigned depth;

    time = fe_GetTimeFromEvent(event);
	
	int delta = (time - fe_click_action_last);

    if (delta <	XtGetMultiClickTime(XtDisplay(widget)))
		n_clicks = 2;
	
    fe_click_action_last = time;
	
    for (n = 0; n + 1 < *ac; n++) {
		
		command_name = NULL;
		
		if (XP_STRCASECMP(av[n], "double") == 0) {
			c_clicks = 2;
			n++;
		} else if (XP_STRCASECMP(av[n], "single") == 0) {
			c_clicks = 1;
			n++;
		} else {
			c_clicks = 1;
		}

		if (XP_STRCMP(av[n], "{") == 0) {
			n++;
		} else {
			// syntax error.
			return;
		}

		// find closing brace.
		for (depth = 1, m = n; av[m] != NULL && m < *ac; m++) {
			if (XP_STRCMP(av[m], "{") == 0)
				depth++;
			else if (XP_STRCMP(av[m], "}") == 0)
				depth--;
			
			if (depth == 0)
				break;
		}

		if (c_clicks == n_clicks && m > n && av[m] != NULL) {
			Cardinal foo = m - n;
			xfeDoCommandAction(widget, event, &av[n], &foo);
			break; // done
		}
	    
		n = m;
    }
}

static void
xfeUndefinedKeyAction(Widget w,
					  XEvent */*event*/,
					  String */*params*/, Cardinal */*num_params*/)
{
	XBell(XtDisplay(w), 0);
}

// Stole this from xp_file.c
static const char *
fe_config_directory(char* buf)
{
  static XP_Bool initted = FALSE;
  const char *dir = ".netscape";
  char *home;
  if (initted)
	return buf;

  home = getenv ("HOME");
  if (!home)
	home = "";

#ifdef OLD_UNIX_FILES

  sprintf (buf, "%.900s", home);
  if (buf[strlen(buf)-1] == '/')
	buf[strlen(buf)-1] = 0;

#else  /* !OLD_UNIX_FILES */

  if (*home && home[strlen(home)-1] == '/')
	sprintf (buf, "%.900s%s", home, dir);
  else
	sprintf (buf, "%.900s/%s", home, dir);

#endif /* !OLD_UNIX_FILES */

  return buf;
}

typedef enum {
  XFE_STAT_ISDIR,
  XFE_STAT_ISREG
} xfeStatType; 

// Stole this from xp_file.c
static int
fe_sprintf_stat( char * buf,
                 const char * dirName,
                 const char * lang,
                 const char * fileName,
                 xfeStatType whichStat)
{
    int               result;
    int               len = 0;
    XP_StatStruct     outStat;

    if (dirName == NULL || 0 == (len = strlen(dirName)) || len > 900)
      return -1;

    while (--len > 0 && dirName[len] == '/')
	/* do nothing */;		/* strip trailing slashes */

    strncpy(buf, dirName, ++len);
    buf[len++] = '/';
    buf[len]   =  0;

    if (lang != NULL && *lang == 0)
      lang  = NULL;

    if (lang != NULL) 
      sprintf(buf+len, "%.100s/%s", lang, fileName);
    else
      strcpy(buf+len, fileName);

    result = stat( buf, &outStat );
    if (result < 0 
        || (whichStat == XFE_STAT_ISDIR && !S_ISDIR(outStat.st_mode))
        || (whichStat == XFE_STAT_ISREG && !S_ISREG(outStat.st_mode))) {
      result = -1;
    }
    if (result < 0 && lang != NULL) {
      result = fe_sprintf_stat(buf, dirName, NULL, fileName, whichStat);
    }
    return result;
}


// Called from XFE_BookmarkView::loadBookmarks() to get the
//   location of the default bookmarks.
//
// Search order:
//    $MOZILLA_HOME/bookmark.htm
//    <program directory>/bookmark.htm
//    /usr/local/lib/netscape/bookmark.htm
//
extern "C"
char *
fe_getDefaultBookmarks(void)
{
#ifdef PATH_MAX
  char bmDir[PATH_MAX];
#else
  char bmDir[1024]; // we probably should use pathconf here
#endif
  char *bmFilename = "bookmark.htm";
  char *mozHome  = getenv("MOZILLA_HOME");
  char *lang     = getenv("LANG");
  char  progDir[1024];
	
  // Setup the URL prefix
	
  fe_GetProgramDirectory(progDir, sizeof progDir);

  if (!fe_sprintf_stat(bmDir, mozHome, lang, bmFilename, XFE_STAT_ISREG)
      || !fe_sprintf_stat(bmDir, progDir, lang, bmFilename, XFE_STAT_ISREG)
      || !fe_sprintf_stat(bmDir, STANDARD_INSTALL_PATH, lang, bmFilename,
                          XFE_STAT_ISREG))
    {
      // Found it
      return XP_STRDUP(bmDir);
    }
  else
    {
      return NULL;
    }
}

// Called from mkhelp.c to get the standard location
//    of the NetHelp folder as a URL
//
// Search order:
//    ~/.netscape/nethelp
//    $NS_NETHELP_PATH
//    $MOZILLA_HOME/nethelp
//    <program directory>/nethelp
//    /usr/local/lib/netscape/nethelp
//
char * FE_GetNetHelpDir()
{
#ifdef PATH_MAX
  char nethelpDir[PATH_MAX+15];
#else
  char nethelpDir[1024+15]; // we probably should use pathconf here
#endif
  int  filePrefixLen;
  char *nethelpSuffix = "nethelp/";
  static char  configBuf[1024];
  const char *conf_dir = fe_config_directory(configBuf);
  char *helpPath = getenv("NS_NETHELP_PATH");
  char *mozHome  = getenv("MOZILLA_HOME");
  char *lang     = getenv("LANG");
  char  progDir[1024];
	
  // Setup the URL prefix
	
  XP_STRCPY(nethelpDir, "file:");
  filePrefixLen = strlen(nethelpDir);
  fe_GetProgramDirectory(progDir, sizeof progDir);

  if (!fe_sprintf_stat(nethelpDir + filePrefixLen, 
                       conf_dir, lang, nethelpSuffix, XFE_STAT_ISDIR) 
      || !fe_sprintf_stat(nethelpDir + filePrefixLen,
                          helpPath, lang, nethelpSuffix, XFE_STAT_ISDIR)
      || !fe_sprintf_stat(nethelpDir + filePrefixLen,
                          mozHome, lang, nethelpSuffix, XFE_STAT_ISDIR)
      || !fe_sprintf_stat(nethelpDir + filePrefixLen,
                          progDir, lang, nethelpSuffix, XFE_STAT_ISDIR)
      || !fe_sprintf_stat(nethelpDir + filePrefixLen,
                          STANDARD_INSTALL_PATH, lang, nethelpSuffix,
                          XFE_STAT_ISDIR))
    {
      // Found it - result already copied into nethelpDir
      ;
    }

  // Found it or not, we return a valid URL
  return XP_STRDUP(nethelpDir);
}



MWContext *FE_GetNetHelpContext()
{
	/* Per: mcafee, use the bookmark context as our dummy context */
	
	MWContext	*pActiveContext = fe_getBookmarkContext();

	return pActiveContext;
}

extern "C" void
fe_Exit(int status)
{
	XFE_MozillaApp::theApp()->exit(status);
}
