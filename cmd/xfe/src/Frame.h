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
   Frame.h -- class definitions for FE top level windows.
   Created: Chris Toshok <toshok@netscape.com>, 7-Aug-96.
 */



#ifndef _xfe_frame_h
#define _xfe_frame_h

#include "structs.h"
#include "xp_core.h"

#ifdef MOZ_TASKBAR
#include "TaskBar.h"
#endif
#include "MenuBar.h"
#include "Component.h"
#include "xfe.h"
#include "mozilla.h"  /* for MWContext ! */
#include "selection.h"
#include "icons.h"

#include "Toolbar.h"
#include "Toolbox.h"

// define or undefine CLEAN_UP_NEWS for the benefit of
// ThreadFrame, MsgFrame and FolderFrame
#define CLEAN_UP_NEWS 1

class XFE_View;
class XFE_Dashboard;
class XFE_Logo;
class XFE_Toolbox;

struct fe_colormap;  /* don't include xfe.h here, since it conflicts with our stuff. */

typedef enum EFrameType {
  FRAME_BROWSER,
  FRAME_DOWNLOAD,
  FRAME_EDITOR,
  FRAME_HTML_DIALOG,
  FRAME_MAILNEWS_COMPOSE,
  FRAME_MAILNEWS_MSG,
  FRAME_MAILNEWS_THREAD,
  FRAME_MAILNEWS_FOLDER,
  FRAME_MAILNEWS_SEARCH,
  FRAME_MAILNEWS_DOWNLOAD,
  FRAME_LDAP_SEARCH,
  FRAME_MAILFILTER,
  FRAME_ADDRESSBOOK,
  FRAME_BOOKMARK,
  FRAME_HISTORY,
  FRAME_NAVCENTER
} EFrameType;

typedef enum {
  XFE_UNSECURE,
  XFE_SECURE,
  XFE_SECURE_MIXED
} XFE_BrowserSecurityStatusType;

typedef enum {
  XFE_UNSECURE_UNSIGNED,
  XFE_SECURE_UNSIGNED,
  XFE_UNSECURE_SIGNED,
  XFE_SECURE_SIGNED
} XFE_MailSecurityStatusType;


class XFE_Frame : public XFE_Component
{
public:
	// friends
	friend void XFE_Frame_busy_timeout(XtPointer, XtIntervalId*);

	XFE_Frame(char *name,
			Widget toplevel,
			XFE_Frame *parent_frame,
			EFrameType type,
			Chrome *chromespec = NULL,
			XP_Bool haveHTMLDisplay = False, // do we display html?  used to initialize colormap stuff
			XP_Bool haveMenuBar = True,   // do we have a menu bar?
			XP_Bool haveToolbars = True,  // do we have toolbars
			XP_Bool haveDashboard = True, // do we have the dashboard?
			XP_Bool destroyOnClose = True); // do we destroy the widget tree on close?

  virtual ~XFE_Frame();


  virtual EFrameType getType();

  virtual void setTitle(char *title);
  virtual void setIconTitle(const char *icon_title);
  virtual void setIconPixmap(fe_icon *icon);

  virtual void    show();
  virtual void    map();
  virtual void    hide();
  virtual XP_Bool isShown();

  virtual void    showTitleBar();
  virtual void    hideTitleBar();
  virtual XP_Bool isTitleBarShown();

  virtual void queryChrome(Chrome * chrome);
  virtual void respectChrome(Chrome * chrome);

  virtual XFE_View *getView();

  virtual const char * getTitle();

  virtual void position(int x, int y);
  virtual void resize(int width, int height);
  virtual void setWidth(int width);
  virtual void setHeight(int height);
  virtual int getX();
  virtual int getY();
  virtual int getWidth();
  virtual int getHeight();

  virtual Widget getViewParent();
  virtual Widget getMainForm();
  virtual Widget getChromeParent();
  virtual Widget getDialogParent();
  virtual fe_colormap *getColormap();
  virtual MWContext *getContext();

  virtual void updateToolbar();
  virtual void updateMenuBar();

	/* this method is used by MozillaApp to turn off accelerators for
	   a particular frame. */
	XP_Bool hotKeysDisabled();

  /* These methods match up nicely to their mates in XFE_View.
     All they basically do is wrap around the subview of this 
     frame. */
  virtual XP_Bool isCommandEnabled(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual void doCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool handlesCommand(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual char *commandToString(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XP_Bool isCommandSelected(CommandType cmd, void *calldata = NULL,
						 XFE_CommandInfo* = NULL);
  virtual XFE_Command* getCommand(CommandType);
  XFE_View*	widgetToView(Widget w);

  virtual Pixel getFGPixel();
  virtual Pixel getBGPixel();
  virtual Pixel getTopShadowPixel();
  virtual Pixel getBottomShadowPixel();

  void setCursor(XP_Bool busy);

  // Invoked just before we destroy the Frame.
  static const char *beforeDestroyCallback;
  // User did something in this frame.
  static const char *userActivityHere;
  // Encoding for this window has changed.
  static const char *encodingChanged;

  // Progress bar cylon notifications
  static const char *progressBarCylonStart;
  static const char *progressBarCylonStop;
  static const char *progressBarCylonTick;

  // Progress bar percent notifications
  static const char *progressBarUpdatePercent;
  static const char *progressBarUpdateText;

  // Logo animation notifications
  static const char *logoStartAnimation;
  static const char *logoStopAnimation;

  // Frame busy notifications.
  static const char *frameBusyCallback;
  static const char *frameNotBusyCallback;

  XFE_CALLBACK_DECL(updateBusyState)
  XFE_CALLBACK_DECL(doCommandCallback)
  XFE_CALLBACK_DECL(toplevelWindowChangeOccured)

  // Animation notifications
  XFE_CALLBACK_DECL(logoAnimationStartNotice)
  XFE_CALLBACK_DECL(logoAnimationStopNotice)

  virtual char *prompt(const char *caption, const char *message, const char *deflt);

  // called when all connections when this window are finished.
  // Really should be a notification...
  virtual void allConnectionsComplete();

  static const char *allConnectionsCompleteCallback;

  static XFE_Frame *getActiveFrame();

  /* called when we want to close a window out from under the user. */
  virtual void app_delete_response();

  /* called when the user wants to close a window. */
  virtual void delete_response();

  // returns False if javascript has said the window is unclosable.
  XP_Bool isWMClosable();

  // recomputes the attachments for our chunks.  If force is false,
  // we put off the layout if the Frame is not popped up, until it
  // is popped up.
  void doAttachments(XP_Bool force = False);

  // Returns the index for the security status icon
  //    (differs between mail & browser)
  virtual int getSecurityStatus();

  void storeProperty (MWContext *context, char *property,
				 const unsigned char *data);

  void installRemoteEventHandler ();
  void frameResizeHandler(int width, int height);

	// tooltips and doc string
	virtual char *getDocString(CommandType cmd);
	virtual char *getTipString(CommandType cmd);

  virtual XFE_Logo *		getLogo				();
  virtual XFE_Dashboard *	getDashboard		();

  virtual XP_Bool isOkToClose();
  virtual void	doClose();
protected:
    char* geometryPrefName;
    // Next two routines return static data -- copy immediately
    // if for some reason you want to keep them.
    char* getWidthPrefString();
    char* getHeightPrefString();

  virtual void setMenubar(MenuSpec *menubar_spec);
  virtual void setToolbar(ToolbarSpec *toolbar_spec);
  virtual void setView(XFE_View *new_view);

  /* These two methods allow frame specific chrome to be added
     by subclasses.  setAboveViewArea() places the widget in
     the form above the view but below the toolbar.  
     setBelowViewArea() places the component below the view but above
     the dashboard. */
  virtual void setAboveViewArea(XFE_Component *above_view);
  virtual void setBelowViewArea(XFE_Component *below_view);

	// this method does all of the initial work from show() -- basically
	// everything but the XtPopup -- but we need it to be separate since
	// the mail download dialog has to do things before it actually is 
	// shown, but after it is realized.
	void realize();

	// this method creates the toplevel shell of this frame.  It handles
	// modality here as well.  So, as an offshoot, we can now support
	// any type of modal window, not just dialogs.
	void createShellAndChromeParent(char *name,
									Widget parent);

  // The bastard catch all structure from hell.  It is here now, since
  // it represents for all the FE code one toplevel window.
  MWContext *m_context;
  void initializeMWContext(EFrameType frame_type, MWContext *context_to_copy = NULL);

  void hackTranslations(Widget widget);
  void hackTextTranslations(Widget widget);

  // Add Z order support
  void zaxis_AddSupport();

  // baseWidget() in case of a frame points to the shell.

  // the widget used as the parent of the view
  Widget m_viewparent; 

  // the widget used as the parent of the m_view_parent, toolbox and above_view
  Widget m_mainform;
  
  // the widget used as the parent for all the chrome.
  Widget m_chromeparent;

  XFE_MenuBar *			m_menubar;
//  XFE_ToolboxItem *		m_toolbar;
  XFE_Toolbar *			m_toolbar;
  XFE_Toolbox *			m_toolbox;
  XFE_Component *		m_aboveview;
  XFE_View *			m_view;
  XFE_Component *		m_belowview;
  XFE_Dashboard *		m_dashboard;


  XFE_Logo *			m_activeLogo;

  fe_colormap *m_cmap;

  void (*m_chrome_close_callback)(void *close_arg);
  void *m_chrome_close_arg;

  // Remember if a chromespec is provided to the constructor so that we 
  // can later ignore the user's toolbox and geometry preferences.
  XP_Bool m_chromespec_provided;
  XP_Bool m_first_showing;


  static void delete_response(Widget, XtPointer, XtPointer);
  static void really_delete(void *data);

  // Toolbox snap notice
  XFE_CALLBACK_DECL(toolboxSnapNotice)

  // Toolbox swap notice
  XFE_CALLBACK_DECL(toolboxSwapNotice)

  // Toolbox close notice
  XFE_CALLBACK_DECL(toolboxCloseNotice)

  // Toolbox open notice
  XFE_CALLBACK_DECL(toolboxOpenNotice)

  // Toolbox methods
  virtual void		toolboxItemSnap			(XFE_ToolboxItem * item);
  virtual void		toolboxItemSwap			(XFE_ToolboxItem * item);
  virtual void		toolboxItemClose		(XFE_ToolboxItem * item);
  virtual void		toolboxItemOpen		(XFE_ToolboxItem * item);
  virtual void		toolboxItemChangeShowing(XFE_ToolboxItem * item);

  virtual void		configureLogo			();
  virtual void		configureToolbox		();

  //
  // There is a one-to-one correspondence between
  // m_allowresize, m_ismodal, m_topmost, m_bottommost, m_zlock
  // and the Chrome struct.  Perhaps in the future we will just
  // include a Chrome struct.
  //
  XP_Bool m_allowresize;
  XP_Bool m_iswmclosable;
  XP_Bool m_ismodal;
  XP_Bool m_hotkeysdisabled;
  int32 m_lhint;
  int32 m_thint;
  XP_Bool m_topmost;    // used by show()
  XP_Bool m_bottommost; // used by show()
  XP_Bool m_zlock;      // used by show()
  XP_Bool m_remoteenabled;

#ifdef NETCASTER_ZAXIS_HACKERY
  time_t m_zaxis_LastStackingChangeTime;
  int m_zaxis_StackingChangesPerSecond;
  XP_Bool m_zaxis_BelowHandlerInstalled;
  XP_Bool m_zaxis_AboveHandlerInstalled;

  void zaxis_InstallHandler(Widget shell,int mode);
  void zaxis_RemoveHandler(Widget shell,int mode);
  void zaxis_HandleEvent(Widget shell,int mode);
  static void zaxis_AboveEH(Widget,XtPointer,XEvent *,Boolean *);
  static void zaxis_BelowEH(Widget,XtPointer,XEvent *,Boolean *);
#endif

  XFE_Frame *m_parentframe;

  EFrameType m_frametype;
  XP_Bool m_haveHTMLDisplay;
  XP_Bool m_destroyOnClose;

  XP_Bool m_attachmentsNeeded; // if we defer the attachments, this will be true.
  Widget m_toplevelWidget; // the parent widget to this window

  static MenuSpec new_menu_spec[];
  static MenuSpec encoding_menu_spec[];
  static MenuSpec window_menu_spec[];
  static MenuSpec help_menu_spec[];
  static MenuSpec places_menu_spec[];
  static MenuSpec tb_places_menu_spec[];

  // shared submenu
  static MenuSpec bookmark_submenu_spec[];

  static MenuSpec new_submenu_spec[];
  static MenuSpec newMsg_submenu_spec[];
  static MenuSpec mark_submenu_spec[];
  static MenuSpec addrbk_submenu_spec[];
  static MenuSpec attachments_submenu_spec[];
  static MenuSpec sort_submenu_spec[];
  static MenuSpec threads_submenu_spec[];
  static MenuSpec expand_collapse_submenu_spec[];
  static MenuSpec headers_submenu_spec[];
  static MenuSpec select_submenu_spec[];
  static MenuSpec reply_submenu_spec[];
  static MenuSpec next_submenu_spec[];
  static MenuSpec compose_message_submenu_spec[];
  static MenuSpec compose_article_submenu_spec[];

private:

  // Event handler used to track the first mapping of iconic frame shells
  static void iconicFrameMappingEH		(Widget,XtPointer,XEvent *,Boolean *);

  // Session manager support (WM_COMMAND)
  static XP_List *	sm_getAliveShellFrames		();
  static char *		sm_getAddressForFrame		(XFE_Frame * frame);
  static void		sm_allocArgvForStringList	(XP_List *,char ***,int *,int);
  static void		sm_freeArgvForStringList	(char **,int);
  static void		sm_freeStringList			(XP_List *);
  static XP_List *	sm_getFrameAddressList		(XP_List *);
  static void		sm_updateSessionCommand		(XFE_Frame * frame);
  static void		sm_saveYourselfCB			(Widget,XtPointer,XtPointer);
  static Widget		sm_getSessionManagerShell	();
  static void		sm_clearCommandProperties	();
  
  void				sm_addSaveYourselfCB		();

  static Atom		sm_wm_command;
  static Atom		sm_wm_save_yourself;
};

//
//    This a wrapper function that controls busy cursor notification, etc.
//    and calls frame->doCommand() to do the real work. It should not
//    be a member function.
//
void xfe_ExecuteCommand(XFE_Frame* frame,
						CommandType cmd,
						void *calldata = NULL,
						XFE_CommandInfo* = NULL);

#endif /* _xfe_frame_h */

