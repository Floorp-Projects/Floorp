/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   mozilla.c --- initialization for the X front end.
   Created: Jamie Zawinski <jwz@netscape.com>, 22-Jun-94.
 */

#include "mozilla.h"
#include "altmail.h"
#include "name.h"
#include "xfe.h"
#include "prefs.h"
#include "net.h"
#include "xp.h"
#include "xp_help.h" /* for XP_NetHelp() */
#include "secnav.h"
#include "ssl.h"
#include "np.h"
#include "outline.h"
#include "mailnews.h"
#include "fonts.h"
#include "secnav.h"
#include "secrng.h"
#include "mozjava.h"
#ifdef NSPR20
#include "private/prpriv.h"	/* for PR_NewNamedMonitor */
#endif /* NSPR20 */

#include "libimg.h"             /* Image Library public API. */

#include "prefapi.h"
#include "hk_funcs.h"

#include "libmocha.h"

#ifdef EDITOR
#include "xeditor.h"
#endif
#include "prnetdb.h"
#ifndef NSPR20
#include "prevent.h"
#else
#include "plevent.h"
#endif
#include "e_kit.h"

#include "bkmks.h"		/* for drag and drop in mail compose */
#include "icons.h"
#include "icondata.h"

#include "felocale.h"

#include "motifdnd.h"

#include "NSReg.h"

#define XFE_RDF
#ifdef XFE_RDF
#include "rdf.h"
#endif

#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <X11/keysym.h>

#ifdef __sgi
# include <malloc.h>
#endif

#if defined(AIXV3) || defined(__osf__)
#include <sys/select.h>
#endif /* AIXV3 */

#include <arpa/inet.h>	/* for inet_*() */
#include <netdb.h>	/* for gethostbyname() */
#include <pwd.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <sys/utsname.h>

#include <sys/types.h>
#include <fcntl.h>

#include <sys/errno.h>

#ifdef UNIX_ASYNC_DNS
#include "xfe-dns.h"
#endif

#ifdef MOZILLA_GPROF
#include "gmon.h"
#endif

#ifndef _STDC_
#define _STDC_ 1
#define HACKED_STDC 1
#endif

#include "src/Netcaster.h"

#include <XmL/Folder.h>

#include <Xfe/Xfe.h>			/* for xfe widgets and utilities */
#include <Xfe/Button.h>

#ifdef HACKED_STDC
#undef HACKED_STDC
#undef _STDC_
#endif

extern int awt_MToolkit_dispatchToWidget(XEvent *xev);
extern XP_Bool fe_IsCalendarInstalled(void);


/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_RESOURCES_NOT_INSTALLED_CORRECTLY;
extern int XFE_USAGE_MSG1;
extern int XFE_USAGE_MSG2;
extern int XFE_USAGE_MSG3;
#if defined(MOZ_MAIL_NEWS) && defined(EDITOR) && defined(TASKBAR)
extern int XFE_USAGE_MSG4;
#endif
extern int XFE_USAGE_MSG5;
extern int XFE_VERSION_COMPLAINT_FORMAT;
extern int XFE_INAPPROPRIATE_APPDEFAULT_FILE;
extern int XFE_INVALID_GEOMETRY_SPEC;
extern int XFE_UNRECOGNISED_OPTION;
extern int XFE_APP_HAS_DETECTED_LOCK;
extern int XFE_ANOTHER_USER_IS_RUNNING_APP;
extern int XFE_APPEARS_TO_BE_RUNNING_ON_HOST_UNDER_PID;
extern int XFE_APPEARS_TO_BE_RUNNING_ON_ANOTHER_HOST_UNDER_PID;
extern int XFE_YOU_MAY_CONTINUE_TO_USE;
extern int XFE_OTHERWISE_CHOOSE_CANCEL;
extern int XFE_EXISTED_BUT_WAS_NOT_A_DIRECTORY;
extern int XFE_EXISTS_BUT_UNABLE_TO_RENAME;
extern int XFE_UNABLE_TO_CREATE_DIRECTORY;
extern int XFE_UNKNOWN_ERROR;
extern int XFE_ERROR_CREATING;
extern int XFE_ERROR_WRITING;
extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_CREATE_CONFIG_FILES;
extern int XFE_OLD_FILES_AND_CACHE;
extern int XFE_OLD_FILES;
extern int XFE_THE_MOTIF_KEYSYMS_NOT_DEFINED;
extern int XFE_SOME_MOTIF_KEYSYMS_NOT_DEFINED;

extern int XFE_SPLASH_REGISTERING_CONVERTERS;
extern int XFE_SPLASH_INITIALIZING_SECURITY_LIBRARY;
extern int XFE_SPLASH_INITIALIZING_NETWORK_LIBRARY;
extern int XFE_SPLASH_INITIALIZING_MESSAGE_LIBRARY;
extern int XFE_SPLASH_INITIALIZING_IMAGE_LIBRARY;
extern int XFE_SPLASH_INITIALIZING_MOCHA;
extern int XFE_SPLASH_INITIALIZING_PLUGINS;

extern int XFE_PREFS_DOWNGRADE;

extern int XFE_MOZILLA_WRONG_RESOURCE_FILE_VERSION;
extern int XFE_MOZILLA_CANNOT_FOND_RESOURCE_FILE;
extern int XFE_MOZILLA_LOCALE_NOT_SUPPORTED_BY_XLIB;
extern int XFE_MOZILLA_LOCALE_C_NOT_SUPPORTED;
extern int XFE_MOZILLA_LOCALE_C_NOT_SUPPORTED_EITHER;
extern int XFE_MOZILLA_NLS_LOSAGE;
extern int XFE_MOZILLA_NLS_PATH_NOT_SET_CORRECTLY;
extern int XFE_MOZILLA_UNAME_FAILED;
extern int XFE_MOZILLA_UNAME_FAILED_CANT_DETERMINE_SYSTEM;
extern int XFE_MOZILLA_TRYING_TO_RUN_SUNOS_ON_SOLARIS;
extern int XFE_MOZILLA_FAILED_TO_INITIALIZE_EVENT_QUEUE;
extern int XFE_MOZILLA_INVALID_REMOTE_OPTION;
extern int XFE_MOZILLA_ID_OPTION_MUST_PRECEED_REMOTE_OPTIONS;
extern int XFE_MOZILLA_ONLY_ONE_ID_OPTION_CAN_BE_USED;
extern int XFE_MOZILLA_INVALID_OD_OPTION;
extern int XFE_MOZILLA_ID_OPTION_CAN_ONLY_BE_USED_WITH_REMOTE;
extern int XFE_MOZILLA_NOT_FOUND_IN_PATH;
extern int XFE_MOZILLA_RENAMING_SOMETHING_TO_SOMETHING;
extern int XFE_MOZILLA_ERROR_SAVING_OPTIONS;

extern int XFE_MOZILLA_XKEYSYMDB_SET_BUT_DOESNT_EXIST;
extern int XFE_MOZILLA_NO_XKEYSYMDB_FILE_FOUND;

extern void fe_showCalendar(Widget toplevel);

extern void fe_createBookmarks(Widget toplevel, void */*XXX XFE_Frame*/, Chrome *chromespec);
extern MWContext* fe_showBookmarks(Widget toplevel, void *parent_frame, Chrome *chromespec);
extern MWContext* fe_showHistory(Widget toplevel, void *parent_frame, Chrome *chromespec);

#ifdef MOZ_MAIL_NEWS
extern MWContext* fe_showInbox(Widget toplevel, void *parent_frame, Chrome *chromespec, XP_Bool with_reuse, XP_Bool getNewMail);
extern MWContext* fe_showNewsgroups(Widget toplevel, void *parent_frame, Chrome *chromespec);
extern Widget fe_MailComposeWin_Create(MWContext* context, Widget parent);
extern void fe_showConference(Widget w, char *email, short use, char *coolAddr);
extern void FE_InitAddrBook();
#endif

extern int XFE_COLORMAP_WARNING_TO_IGNORE;

#if defined(__sun) && !defined(__svr4__) && (XtSpecificationRelease != 5)
ERROR!!  Must use X11R5 with Motif 1.2 on SunOS 4.1.3!
#endif

/* This means "suppress the splash screen if there is was a URL specified
   on the command line."  With betas, we always want the splash screen to
   be printed to make sure the user knows it's beta. */
#define NON_BETA

/* Allow a nethelp startup flag */
#define NETHELP_STARTUP_FLAG

const char *fe_progname_long;
const char *fe_progname;
const char *fe_progclass;
XtAppContext fe_XtAppContext;
void (*old_xt_warning_handler) (String nameStr, String typeStr, String classStr,
	String defaultStr, String* params, Cardinal* num_params);
XtErrorHandler old_xt_warningOnly_handler;
Display *fe_display = NULL;
Atom WM_SAVE_YOURSELF; /* For Session Manager */

char *fe_pidlock;

/* Polaris components */
static void fe_InitPolarisComponents(char *);
char *fe_calendar_path = 0;
char *fe_host_on_demand_path = 0;

/* Conference */
static void fe_InitConference(char *);
char *fe_conference_path = 0;

static int
fe_create_pidlock (const char *name, unsigned long *paddr, pid_t *ppid);

static void fe_check_use_async_dns(void);

XP_Bool fe_ReadLastUserHistory(char **hist_entry_ptr);

#ifdef DEBUG
int Debug;
#endif

PRMonitor *fdset_lock;
PRThread *mozilla_thread;
PREventQueue* mozilla_event_queue;

/* Begin - Session Manager stuff */

int  save_argc = 0;
char **save_argv = NULL;

void fe_wm_save_self_cb(Widget, XtPointer, XtPointer);
static const char* fe_locate_program_path(const char* progname);
static char* fe_locate_component_path(const char* fe_prog, char*comp_name);

/* End - Session Manager stuff */


void fe_sec_logo_cb (Widget, XtPointer, XtPointer);

#ifndef OLD_UNIX_FILES
static XP_Bool fe_ensure_config_dir_exists (Widget toplevel);
static void fe_copy_init_files (Widget toplevel);
static void fe_clean_old_init_files (Widget toplevel);
#else  /* OLD_UNIX_FILES */
static void fe_rename_init_files (Widget toplevel);
static char *fe_renamed_cache_dir = 0;
#endif /* OLD_UNIX_FILES */

static void fe_read_screen_for_rng (Display *dpy, Screen *screen);
static int x_fatal_error_handler(Display *dpy);
static int x_error_handler (Display *dpy, XErrorEvent *event);
static void xt_warning_handler(String nameStr, String typeStr, String classStr,
	String defaultStr, String* params, Cardinal* num_params);
static void xt_warningOnly_handler(String msg);
void fe_GetProgramDirectory(char *path, int len);

#if defined(NSPR) && defined(TOSHOK_FIXES_THE_SPLASH_SCREEN_HANGS)
#define NSPR_SPLASH
#endif

#ifdef NSPR_SPLASH
extern void fe_splashStart(Widget);
extern void fe_splashUpdateText(char *text);
extern void fe_splashStop(void);
#endif

extern void fe_startDisplayFactory(Widget toplevel);

static XrmOptionDescRec options [] = {
  { "-geometry",	".geometry",		   XrmoptionSepArg, 0 },
  { "-visual",		".TopLevelShell.visualID", XrmoptionSepArg, 0 },
  { "-ncols",		".maxImageColors", 	   XrmoptionSepArg, 0 },
  { "-iconic",		".iconic",		   XrmoptionNoArg, "True" },
  { "-install",		".installColormap",	   XrmoptionNoArg, "True" },
  { "-noinstall",	".installColormap",	   XrmoptionNoArg, "False" },
  { "-no-install",	".installColormap",	   XrmoptionNoArg, "False" },
  { "-netcaster",	".startNetcaster",	   XrmoptionNoArg, "True" },
#ifdef USE_NONSHARED_COLORMAPS
  { "-share",		".shareColormap",	   XrmoptionNoArg, "True" },
  { "-noshare",		".shareColormap",	   XrmoptionNoArg, "False" },
  { "-no-share",	".shareColormap",	   XrmoptionNoArg, "False" },
#endif
  { "-mono",		".forceMono",		   XrmoptionNoArg, "True" },
  { "-gravity-works",	".windowGravityWorks",	   XrmoptionNoArg, "True" },
  { "-broken-gravity",	".windowGravityWorks",	   XrmoptionNoArg, "False" },

  { "-xrm",		0,			   XrmoptionResArg, 0 },
#ifdef NSPR_SPLASH
  { "-splash",		".showSplash",		   XrmoptionNoArg, "True" },
  { "-nosplash",	".showSplash",		   XrmoptionNoArg, "False" },
#endif

#ifdef MOZ_TASKBAR
  /* Startup component flags */
  { "-component-bar",		".componentBar",	XrmoptionNoArg, "True" },
#endif
#ifdef MOZ_MAIL_NEWS

  { "-composer",			".composer",		XrmoptionNoArg, "True" },
  { "-edit",				".composer",		XrmoptionNoArg, "True" },

  { "-messenger",			".mail",			XrmoptionNoArg, "True" },
  { "-mail",				".mail",			XrmoptionNoArg, "True" },

#endif

  { "-bookmarks",			".bookmarks",		XrmoptionNoArg, "True" },
  { "-history",				".history",			XrmoptionNoArg, "True" },

#ifdef NETHELP_STARTUP_FLAG
  { "-nethelp",				".nethelp",			XrmoptionNoArg, "True" },
#endif

  { "-discussions",			".news",			XrmoptionNoArg, "True" },
  { "-news",				".news",			XrmoptionNoArg, "True" },

  { "-dont-save-geometry-prefs",	".dontSaveGeometryPrefs",	XrmoptionNoArg, "True"  },
  { "-ignore-geometry-prefs",		".ignoreGeometryPrefs",		XrmoptionNoArg, "True"  },

  { "-no-about-splash",				".noAboutSplash",			XrmoptionNoArg, "True"  },

  /* Turn session management on/off */
  { "-session-management",			".sessionManagement",		XrmoptionNoArg, "True"  },
  { "-no-session-management",		".sessionManagement",		XrmoptionNoArg, "False" },

  /* Turn IRIX session management on/off */
  { "-irix-session-management",		".irixSessionManagement",	XrmoptionNoArg, "True"  },
  { "-no-irix-session-management",	".irixSessionManagement",	XrmoptionNoArg, "False" },

  { "-dont-force-window-stacking",	".dontForceWindowStacking",	XrmoptionNoArg, "True" },
};

extern char *fe_fallbackResources[];

XtResource fe_Resources [] =
{
  { "linkForeground", XtCForeground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, link_pixel), XtRString, XtDefaultForeground },
  { "vlinkForeground", XtCForeground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, vlink_pixel), XtRString, XtDefaultForeground },
  { "alinkForeground", XtCForeground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, alink_pixel), XtRString, XtDefaultForeground },

  { "selectForeground", XtCForeground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, select_fg_pixel), XtRString,
    XtDefaultForeground },
  { "selectBackground", XtCBackground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, select_bg_pixel), XtRString,
    XtDefaultBackground },

  { "textBackground", XtCBackground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, text_bg_pixel), XtRString,
    XtDefaultBackground },

  { "defaultForeground", XtCForeground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, default_fg_pixel), XtRString,
    XtDefaultForeground },
  { "defaultBackground", XtCBackground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, default_bg_pixel), XtRString,
    XtDefaultBackground },

  { "defaultBackgroundImage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_ContextData *, default_background_image),
    XtRImmediate, "" },

#ifndef NO_SECURITY
  { "secureDocumentColor", XtCForeground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, secure_document_pixel), XtRString, "green" },
  { "insecureDocumentColor", XtCForeground, XtRPixel, sizeof (String),
    XtOffset (fe_ContextData *, insecure_document_pixel), XtRString, "red" },
#endif /* ! NO_SECURITY */

  { "linkCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, link_cursor), XtRString, "hand2" },
  { "busyCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, busy_cursor), XtRString, "watch" },

  { "saveNextLinkCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, save_next_link_cursor), XtRString, "hand2" },
  { "saveNextNonlinkCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, save_next_nonlink_cursor),
    XtRString, "crosshair" },
#ifdef EDITOR
  { "editableTextCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, editable_text_cursor),
    XtRString, "xterm" },
  { "tableSelectionCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, tab_sel_cursor),
    XtRString, "top_left_corner" },
  { "rowSelectionCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, row_sel_cursor),
    XtRString, "right_side" },
  { "columnSelectionCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, col_sel_cursor),
    XtRString, "bottom_side" },
  { "cellSelectionCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, cel_sel_cursor),
    XtRString, "top_left_corner" },
  { "resizeColumnCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, resize_col_cursor),
    XtRString, "left_side" },
  { "resizeTableCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, resize_tab_cursor),
    XtRString, "left_side" },
  { "addColumnCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, add_col_cursor),
    XtRString, "right_side" },
  { "addRowCursor", XtCCursor, XtRCursor, sizeof (Cursor),
    XtOffset (fe_ContextData *, add_row_cursor),
    XtRString, "right_side" },
#endif

  { "confirmExit", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_ContextData *, confirm_exit_p), XtRImmediate,
    (XtPointer) True },

  { "progressInterval", "Seconds", XtRCardinal, sizeof (int),
    XtOffset (fe_ContextData *, progress_interval),
    XtRImmediate, (XtPointer) 1 },
  { "busyBlinkRate", "Microseconds", XtRCardinal, sizeof (int),
    XtOffset (fe_ContextData *, busy_blink_rate),
    XtRImmediate, (XtPointer) 500 },
  { "hysteresis", "Pixels", XtRCardinal, sizeof (int),
    XtOffset (fe_ContextData *, hysteresis), XtRImmediate, (XtPointer) 5 },
  { "blinkingEnabled", "BlinkingEnabled", XtRBoolean, sizeof (Boolean),
    XtOffset (fe_ContextData *, blinking_enabled_p), XtRImmediate,
    (XtPointer) True }
};
Cardinal fe_ResourcesSize = XtNumber (fe_Resources);

XtResource fe_GlobalResources [] =
{
  { "encodingFilters", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, encoding_filters), XtRImmediate, "" },

  { "saveHistoryInterval", "Seconds", XtRCardinal, sizeof (int),
    XtOffset (fe_GlobalData *, save_history_interval), XtRImmediate,
    (XtPointer) 600 },

  { "terminalTextTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, terminal_text_translations),
    XtRImmediate, 0 },
  { "nonterminalTextTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, nonterminal_text_translations),
    XtRImmediate, 0 },

  { "globalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, global_translations),
    XtRImmediate, 0 },
  { "globalTextFieldTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, global_text_field_translations),
    XtRImmediate, 0 },
  { "globalNonTextTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, global_nontext_translations),
    XtRImmediate, 0 },

  { "editingTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, editing_translations),
    XtRImmediate, 0 },
  { "singleLineEditingTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, single_line_editing_translations),
    XtRImmediate, 0 },
  { "multiLineEditingTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, multi_line_editing_translations),
    XtRImmediate, 0 },
  { "formElemEditingTranslations", XtCTranslations, /* form text field elem only */
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, form_elem_editing_translations),
    XtRImmediate, 0 },

  { "browserGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, browser_global_translations),
    XtRImmediate, 0 },
  { "abGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, ab_global_translations),
    XtRImmediate, 0 },
  { "bmGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, bm_global_translations),
    XtRImmediate, 0 },
  { "mnsearchGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, mnsearch_global_translations),
    XtRImmediate, 0 },
  { "ghGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, gh_global_translations),
    XtRImmediate, 0 },
  { "mailnewsGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, mailnews_global_translations),
    XtRImmediate, 0 },
  { "messagewinGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, messagewin_global_translations),
    XtRImmediate, 0 },
  { "mailcomposeGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, mailcompose_global_translations),
    XtRImmediate, 0 },
  { "addressOutlinerTraverseTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, address_outliner_traverse_translations),
    XtRImmediate, 0 },
  { "addressOutlinerKeyTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, address_outliner_key_translations),
    XtRImmediate, 0 },
  { "dialogGlobalTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, dialog_global_translations),
    XtRImmediate, 0 },
  { "editorTranslations", XtCTranslations,
    XtRTranslationTable, sizeof (XtTranslations),
    XtOffset (fe_GlobalData *, editor_global_translations),
    XtRImmediate, 0 },

  { "forceMono", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, force_mono_p), XtRImmediate,
    (XtPointer) False },
  { "wmIconPolicy", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, wm_icon_policy), XtRImmediate,
    (XtPointer) NULL },
#ifdef USE_NONSHARED_COLORMAPS
  { "shareColormap", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, windows_share_cmap), XtRImmediate,
    (XtPointer) False },
#endif
  { "maxImageColors", "Integer", XtRCardinal, sizeof (int),
    XtOffset (fe_GlobalData *, max_image_colors), XtRImmediate,
    (XtPointer) 0 },

#ifdef __sgi
  { "sgiMode", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, sgi_mode_p), XtRImmediate,
    (XtPointer) False },
#endif /* __sgi */

  { "useStderrDialog", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, stderr_dialog_p), XtRImmediate,
    (XtPointer) True },
  { "useStdoutDialog", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, stdout_dialog_p), XtRImmediate,
    (XtPointer) True },

  /* #### move to prefs */
  { "documentColorsHavePriority", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, document_beats_user_p), XtRImmediate,
    (XtPointer) True },

  /* #### move to prefs */
  { "languageRegionList", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, language_region_list), XtRImmediate, "" },
  { "invalidLangTagFormatMsg", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, invalid_lang_tag_format_msg), XtRImmediate, "" },
  { "invalidLangTagFormatDialogTitle", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, invalid_lang_tag_format_dialog_title), 
    XtRImmediate, "" },

# define RES_ERROR "ERROR: Resources not installed correctly!"
  { "noDocumentLoadedMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, no_url_loaded_message),
    XtRImmediate, RES_ERROR },
  { "optionsSavedMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, options_saved_message),
    XtRImmediate, RES_ERROR },
  { "clickToSaveMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, click_to_save_message),
    XtRImmediate, RES_ERROR },
  { "clickToSaveCancelledMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, click_to_save_cancelled_message),
    XtRImmediate, RES_ERROR },
  { "noNextURLMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, no_next_url_message),
    XtRImmediate, RES_ERROR },
  { "noPreviousURLMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, no_previous_url_message),
    XtRImmediate, RES_ERROR },
  { "noHomeURLMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, no_home_url_message),
    XtRImmediate, RES_ERROR },
  { "notOverImageMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, not_over_image_message),
    XtRImmediate, RES_ERROR },
  { "notOverLinkMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, not_over_link_message),
    XtRImmediate, RES_ERROR },
  { "noSearchStringMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, no_search_string_message),
    XtRImmediate, RES_ERROR },
  { "wrapSearchMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, wrap_search_message),
    XtRImmediate, RES_ERROR },
  { "wrapSearchBackwardMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, wrap_search_backward_message),
    XtRImmediate, RES_ERROR },
  { "wrapSearchNotFoundMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, wrap_search_not_found_message),
    XtRImmediate, RES_ERROR },
  { "noAddressesMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, no_addresses_message),
    XtRImmediate, RES_ERROR },
  { "noFileMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, no_file_message),
    XtRImmediate, RES_ERROR },
  { "noPrintCommandMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, no_print_command_message),
    XtRImmediate, RES_ERROR },
  { "bookmarksChangedMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, bookmarks_changed_message),
    XtRImmediate, RES_ERROR },
  { "bookmarkConflictMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, bookmark_conflict_message),
    XtRImmediate, RES_ERROR },
  { "bookmarksNoFormsMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, bookmarks_no_forms_message),
    XtRImmediate, RES_ERROR },
  { "reallyQuitMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, really_quit_message),
    XtRImmediate, RES_ERROR },
  { "doubleInclusionMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, double_inclusion_message),
    XtRImmediate, RES_ERROR },
  { "expireNowMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, expire_now_message),
    XtRImmediate, RES_ERROR },
  { "clearMemCacheMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, clear_mem_cache_message),
    XtRImmediate, RES_ERROR },
  { "clearDiskCacheMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, clear_disk_cache_message),
    XtRImmediate, RES_ERROR },
  { "createCacheDirErrorMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, create_cache_dir_message),
    XtRImmediate, RES_ERROR },
  { "createdCacheDirMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, created_cache_dir_message),
    XtRImmediate, RES_ERROR },
  { "cacheNotDirMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, cache_not_dir_message),
    XtRImmediate, RES_ERROR },
  { "cacheSuffixMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, cache_suffix_message),
    XtRImmediate, RES_ERROR },
  { "cubeTooSmallMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, cube_too_small_message),
    XtRImmediate, RES_ERROR },
  { "renameInitFilesMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, rename_files_message),
    XtRImmediate, RES_ERROR },
  { "overwriteFileMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, overwrite_file_message),
    XtRImmediate, RES_ERROR },
  { "unsentMailMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, unsent_mail_message),
    XtRImmediate, RES_ERROR },
  { "binaryDocumentMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, binary_document_message),
    XtRImmediate, RES_ERROR },
  { "defaultCharset", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, default_url_charset),
    XtRImmediate, "" },
  { "emptyMessageQuestion", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, empty_message_message),
    XtRImmediate, RES_ERROR },
  { "defaultMailtoText", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, default_mailto_text),
    XtRImmediate, "" },
  { "helperAppDeleteMessage", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, helper_app_delete_message),
    XtRImmediate, "" },

#ifdef MOZ_TASKBAR
  /* Startup component flags */
  { "componentBar", XtCBoolean, XtRBoolean, sizeof(Boolean),
	XtOffset (fe_GlobalData *, startup_component_bar), XtRImmediate,
	(XtPointer) False },
#endif
#ifdef MOZ_MAIL_NEWS
  { "composer", XtCBoolean, XtRBoolean, sizeof(Boolean),
	XtOffset (fe_GlobalData *, startup_composer), XtRImmediate,
	(XtPointer) False },

  { "mail", XtCBoolean, XtRBoolean, sizeof(Boolean),
	XtOffset (fe_GlobalData *, startup_mail), XtRImmediate,
	(XtPointer) False },

  { "news", XtCBoolean, XtRBoolean, sizeof(Boolean),
	XtOffset (fe_GlobalData *, startup_news), XtRImmediate,
	(XtPointer) False },

#endif

#ifdef NETHELP_STARTUP_FLAG
  { "nethelp", XtCBoolean, XtRBoolean, sizeof(Boolean),
	XtOffset (fe_GlobalData *, startup_nethelp), XtRImmediate,
	(XtPointer) False },
#endif

  { "history", XtCBoolean, XtRBoolean, sizeof(Boolean),
	XtOffset (fe_GlobalData *, startup_history), XtRImmediate,
	(XtPointer) False },

  { "bookmarks", XtCBoolean, XtRBoolean, sizeof(Boolean),
	XtOffset (fe_GlobalData *, startup_bookmarks), XtRImmediate,
	(XtPointer) False },

  { "iconic", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, startup_iconic), XtRImmediate,
    (XtPointer) False },

  { "geometry", XtCString, XtRString, sizeof (String),
    XtOffset (fe_GlobalData *, startup_geometry), XtRString,
    "" },

  { "dontSaveGeometryPrefs", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, dont_save_geom_prefs), XtRImmediate,
    (XtPointer) False },

  { "ignoreGeometryPrefs", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, ignore_geom_prefs), XtRImmediate,
    (XtPointer) False },

  { "noAboutSplash", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, startup_no_about_splash), XtRImmediate,
    (XtPointer) False },


  /* Session management is on by default */
  { "sessionManagement", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, session_management), XtRImmediate,
    (XtPointer) True },

  /* IRIX Session management should only be on by default on irix platforms */
  { "irixSessionManagement", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, irix_session_management), XtRImmediate,
#if (defined(IRIX))
    (XtPointer) True },
#else
    (XtPointer) False },
#endif
  { "dontForceWindowStacking", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, dont_force_window_stacking), XtRImmediate,
    (XtPointer) False },

#ifdef NSPR_SPLASH
  { "showSplash", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, show_splash), XtRImmediate,
    (XtPointer) True },
#endif
  { "editorUpdateDelay", "ms", XtRCardinal, sizeof (Cardinal),
    XtOffset (fe_GlobalData *, editor_update_delay), XtRImmediate, 
	(XtPointer) 20
  },
  { "editorImInputEnabled", XtCBoolean, XtRBoolean, sizeof (Boolean),
    XtOffset (fe_GlobalData *, editor_im_input_enabled), XtRImmediate,
#if (defined(IRIX) && !defined(IRIX6_2) && !defined(IRIX6_3))
    (XtPointer) False },
#else
    (XtPointer) True }
#endif

# undef RES_ERROR
};
Cardinal fe_GlobalResourcesSize = XtNumber (fe_GlobalResources);

fe_GlobalData   fe_globalData   = { 0, };
XFE_GlobalPrefs fe_globalPrefs  = { 0, };
XFE_GlobalPrefs fe_defaultPrefs = { 0, };

static void
usage (void)
{
  fprintf (stderr, XP_GetString( XFE_USAGE_MSG1 ), fe_long_version + 4,
  				fe_progname);

#ifdef USE_NONSHARED_COLORMAPS
  fprintf (stderr,  XP_GetString( XFE_USAGE_MSG2 ) );
#endif

  fprintf (stderr, XP_GetString( XFE_USAGE_MSG3 ) );

#if defined(MOZ_MAIL_NEWS) && defined(MOZ_TASKBAR) && defined(EDITOR)
  fprintf (stderr, XP_GetString( XFE_USAGE_MSG4 ) );
#endif

  fprintf (stderr, XP_GetString( XFE_USAGE_MSG5 ) );
}

/*******************
 * Signal handlers *
 *******************/
void fe_sigchild_handler(int signo)
{
  pid_t pid;
  int status = 0;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
#ifdef DEBUG_dp
    fprintf(stderr, "fe_sigchild_handler: Reaped pid %d.\n", pid);
#endif
  }
}

/* Netlib re-enters itself sometimes, so this has to be a counter not a flag */
static int fe_netlib_hungry_p = 0;

void
XFE_SetCallNetlibAllTheTime (MWContext * context)
{
  fe_netlib_hungry_p++;
}

void
XFE_ClearCallNetlibAllTheTime (MWContext * context)
{
  --fe_netlib_hungry_p;
  XP_ASSERT(fe_netlib_hungry_p >= 0);
}

void
XP_SetCallNetlibAllTheTime (MWContext * context)
{
  fe_netlib_hungry_p++;
}

void
XP_ClearCallNetlibAllTheTime (MWContext * context)
{
  --fe_netlib_hungry_p;
  XP_ASSERT(fe_netlib_hungry_p >= 0);
}

static void  fe_EventForRNG (XEvent *event);
/* Use a static so that the difference between the running X clock
   and the time that we are about to block is a function of the
   amount of time the last processing took.
 */
static XEvent fe_last_event;


/* Hack to determine whether a Motif drag operation is in progress.
 * This also catches menu system activity, but that should be cool.
 */
#include <Xm/DisplayP.h>
#define IsMotifDragActive(dpy) ( \
                                    ((XmDisplayRec*)XmGetXmDisplay(dpy))->display.userGrabbed \
                                    ? TRUE : FALSE )

/* Process events. The idea here is to give X events priority over
   file descriptor input so that the user gets better interactive
   response under heavy i/o load.

   While we are it, we try to gather up a bunch of events to shovel
   into the security library to "improve" randomness.

   Finally, we try to block when there is really nothing to do,
   but we block in such a way as to allow java applets to get the
   cpu while we are blocked.

   Simple, eh? If you don't understand it, don't touch it. Instead
   find kipp.
   */
void
fe_EventLoop ()
{
  XtInputMask pending;
  static unsigned long device_event_count = 0;
  XP_Bool haveEvent;		/* Used only in debugging sessions. */
#ifdef DEBUG
  int first_time = 1;
  static int entry_depth = 0;
  static int spinning_wildly = 0;

  entry_depth++;
#endif

  /* Feed the security library the last event we handled. We do this here
     before peeking for another event so that the elapsed time between calls
     can have some impact on the outcome.
     */
  fe_EventForRNG (&fe_last_event);

  /* Release X lock and wait for an event to show up. This might not return
     an X event, and it will not block until there is an X event. However,
     it will block (i.e. call select) if there is no X events, timer events
     or i/o events immediately pending. Which is what we want it to do.

     XtAppPeekEvent promises to return after something is ready:
     i/o, x event or xt timer. It returns a value that indicates
     if the event structure passed to it gets filled in (we don't care
     so we ignore the value). The locking is now done inside select - Radha
     */
  /*   PR_XUnlock(); */
  haveEvent = XtAppPeekEvent(fe_XtAppContext, &fe_last_event);
  /*   PR_XLock();  */

  /* Consume all the X events and timer events right now. As long as
     there are X events or Xt timer events, we will sit and spin here.
     As soon as those are exhausted we fall out of the loop.
     */
  for (;;) {
      /* fe_EventLoop()'s stratified processing of events/timers/input
       * can confuse Motif Drag+Drop, and result in permanent window
       * droppings: Motif fires off its own drag event loop from
       * a timer proc, but there is a few-milliseconds window between
       * XmDragStart() and the timer firing. If a user ButtonRelease
       * event arrives and is processed by fe_EventLoop() during that window,
       * then the cursor window dropping is left behind until the entire
       * application quits..
       *
       * So, detect the start of a drag, and revert to a spick+span Xt event
       * loop until the timer for the Motif drag event loop kicks in.
       */
      if (fe_display && IsMotifDragActive(fe_display)) {
          XEvent event;
          while (IsMotifDragActive(fe_display)) {
              XtAppNextEvent(fe_XtAppContext,&event);
              XtDispatchEvent(&event);
          }
          /* all done, let the funkiness continue. */
      }

      
      /* Get a mask of the pending events, timers and i/o streams that
	 are ready for processing. This event will not block, but instead
	 will return 0 when there is nothing pending remaining.
	 */
      pending = XtAppPending(fe_XtAppContext);
      if (pending == 0) {
#ifdef DEBUG
	  /* Nothing to do anymore. Time to leave. We ought to be able to
	     assert that first_time is 0; that is, that we have
	     processed at least one event before breaking out of the loop.
	     Such an assert would help us detect busted Xt libraries.  However,
	     in reality, we have already determined that we do have such busted
	     libraries.  So, we'll assert that this doesn't happen more than
	     once in a row, so that we know we don't really spin wildly.
	     */
	  if (first_time) {
#ifndef I_KNOW_MY_Xt_IS_BAD
#ifdef NSPR20
		/*
		 * XXX - TO BE FIXED
		 * with NSPR20 this count seems to go as high as 7 or 8; needs to
		 * be investigated further
		 */
	      XP_ASSERT(spinning_wildly < 12);
#else
	      XP_ASSERT(spinning_wildly < 3);
#endif
#endif /* I_KNOW_MY_Xt_IS_BAD */
	      spinning_wildly++;
	  }
#endif
	  break;
      }
#ifdef DEBUG
      /* Now we have one event. It's no longer our first time */
      first_time = 0;
      spinning_wildly = 0;
#endif
      if (pending & XtIMXEvent) {
#ifdef JAVA
	  extern int awt_MToolkit_dispatchToWidget(XEvent *);
#endif
	  /* We have an X event to process. We check them first because
	     we want to be as responsive to the user as possible.
	     */
	  if (device_event_count < 300) {
	      /* While the security library is still hungry for events
		 we feed it all the events we get. We use XtAppPeekEvent
		 to grab a copy of the event that XtAppPending said was
		 available.
		 */
	      XtAppPeekEvent(fe_XtAppContext, &fe_last_event);
	      fe_EventForRNG (&fe_last_event);

	      /* If it's an interesting event, count it towards our
		 goal of 300 events. After 300 events, we don't need
		 to feed the security library as often (which makes
		 us more efficient at event processing)
		 */
	      if (fe_last_event.xany.type == ButtonPress ||
		  fe_last_event.xany.type == ButtonRelease ||
		  fe_last_event.xany.type == KeyPress ||
		  fe_last_event.xany.type == MotionNotify)
	      {
		  device_event_count++;
	      }
	  }
#ifdef JAVA
 	  else {
 	      XtAppPeekEvent(fe_XtAppContext, &fe_last_event);
 	  }
 	  /* Allow Java to filter this event. If java doesn't
	  ** consume the event, give it to mozilla
	  */
 	  if (awt_MToolkit_dispatchToWidget(&fe_last_event) == 0) {
#endif    /* JAVA */
	  /* Feed the X event to Xt, being careful to not allow it to
	     process timers or i/o. */
	  XtAppProcessEvent(fe_XtAppContext, XtIMXEvent);

#ifdef JAVA
 	  } else {
 	      /* Discard event since Java has processed it.
 		 Don't use XtAppNextEvent here, otherwise Xt will
 		 spin wildly (at least on IRIX). -- erik
 		 */
	            XNextEvent(fe_display, &fe_last_event); 
	  }
#endif   /* JAVA */
      } else if (pending & XtIMTimer) {
	  /* Let Xt dispatch the Xt timer event. For this case we fall out
	     of the loop in case a very fast timer was set.
	     */
	  XtAppProcessEvent(fe_XtAppContext, XtIMTimer);
	  break;
      } else {
	  /* Go ahead and process the i/o event XtAppPending has
	     reported. However, as soon as it is done fall out of
	     the loop. This way we return to the caller after
	     doing something.
	     */
	  XtAppProcessEvent(fe_XtAppContext, pending);

#ifdef __sgi
	  /*
	   * The SGI version of Xt has a bug where XtAppPending will mark
	   * an internal data structure again when it sees input pending on
	   * the Asian input method socket, even though XtAppProcessEvent
	   * has already put a dummy X event with zero keycode on the X
	   * event queue, which is supposed to tell Xlib to read the IM
	   * socket (when XFilterEvent is called by Xt).
	   *
	   * So we process the dummy event immediately after it has been
	   * put on the queue, so that XtAppPending doesn't mark its data
	   * structure again.
	   */
	  while (XPending(fe_display)) {
	      XtAppProcessEvent(fe_XtAppContext, XtIMXEvent);
	  }
#endif /* __sgi */

	  break;
      }
  }

#ifdef JAVA
{
   extern void awt_MToolkit_finishModals(void);
   awt_MToolkit_finishModals();
}
#endif /* JAVA */

  /* If netlib is anxious, service it after every event, and service it
     continuously while there are no events. */
  if (fe_netlib_hungry_p)
    do
    {
#ifdef QUANTIFY
quantify_start_recording_data();
#endif /* QUANTIFY */
      NET_ProcessNet (-1, NET_EVERYTIME_TYPE);
#ifdef QUANTIFY
quantify_stop_recording_data();
#endif /* QUANTIFY */
#ifdef MOZ_NEO
      /* Currently just to give NeoAccess some cycles - whether it does
         anything, and what it does needs investigation. Also, this really
         needs to be a NeoOnIdle when other neoaccess components come on line
       */
      MSG_OnIdle();  
#endif
    }
    while (fe_netlib_hungry_p && !XtAppPending (fe_XtAppContext));
#ifdef DEBUG
  entry_depth--;
#endif
}

static void
fe_EventForRNG (XEvent *event)
{
  struct
  {
    XEvent event;
    unsigned char noise[16];
  } data;

  data.event = *event;

  /* At this point, we are idle.  Get a high-res clock value. */
  (void) RNG_GetNoise(data.noise, sizeof(data.noise));

  /* Kick security library random number generator to make it very
     hard for a bad guy to predict where the random number generator
     is at.  Initialize it with the current time, and the *previous*
     X event we read (which happens to have a server timestamp in it.)
     The X event came from before this current idle period began, and
     will be uninitialized stack data the first time through.
   */
  RNG_RandomUpdate(&data, sizeof(data));
}


static void
fe_splash_timer (XtPointer closure, XtIntervalId *id)
{
  Boolean *flag = (Boolean *) closure;
  *flag = True;
}

char *sploosh = 0;
static Boolean plonkp = False;
static Boolean plonkp_cancel = False;

/* provide an entry point to cancel plonk's load of initial window */

void
plonk_cancel()
{
    plonkp_cancel = True;
}

/* provide query of whether plonk was cancelled by a user action */

Boolean
plonk_cancelled(void)
{
    return plonkp_cancel;
}

Boolean
plonk (MWContext *context)
{
  int flags = 0;
  int win = False;
  Boolean timed_out = False;
  XtIntervalId id;
  Display *dpy = XtDisplay (CONTEXT_WIDGET (context));
  char *ab = "\246\247\264\272\271\177\270\265\261\246\270\255";
  char *au ="\246\247\264\272\271\177\246\272\271\255\264\267\270";
  char *blank = "\246\247\264\272\271\177\247\261\246\263\260";
  char *tmp;

  plonkp_cancel=False;

  /* Do not plonk if this flag is set */
  if (fe_globalData.startup_no_about_splash)
  {
	  return False;
  }
  
  if ((context->type != MWContextBrowser &&
	context->type != MWContextMail &&
	context->type != MWContextNews)
      || (context->restricted_target)
      )
    return False;

  if (plonkp)
    return False;
  plonkp = True;
  if (sploosh) free (sploosh);
  sploosh = strdup (ab);
  for (tmp = sploosh; *tmp; tmp++) *tmp -= 69;
  fe_GetURL (context, NET_CreateURLStruct (sploosh, NET_DONT_RELOAD), FALSE);

  id = XtAppAddTimeOut (fe_XtAppContext, 60 * 1000, fe_splash_timer,
			&timed_out);

  /* #### Danger, this is weird and duplicates much of the work done
     in fe_EventLoop ().
   */
  while (! timed_out)
    {
      XtInputMask pending = XtAppPending (fe_XtAppContext);

      /* AAAAAUUUUGGHHH!!!  Sleep for a second, or until an X event arrives,
	 and then loop until some input of any type has arrived.   The Xt event
	 model completely **** and I'm tired of trying to figure out how to
	 implement this without polling.
       */
      while (! pending)
	{
	  fd_set fds;
	  int fd = ConnectionNumber (dpy);
	  int usecs = 1000000L;
	  struct timeval tv;
	  tv.tv_sec  = usecs / 1000000L;
	  tv.tv_usec = usecs % 1000000L;
	  memset(&fds, 0, sizeof(fds));
	  FD_SET (fd, &fds);
	  (void) select (fd + 1, &fds, 0, 0, &tv);
	  pending = XtAppPending (fe_XtAppContext);
	}

      if (pending & XtIMXEvent)
	do
	  {
	    int nf = 0;
	    XEvent event;
	    XtAppNextEvent (fe_XtAppContext, &event);

	    if (event.xany.type == KeyPress /* ||
		event.xany.type == KeyRelease */)
	      {
		KeySym s = XKeycodeToKeysym (dpy, event.xkey.keycode, 0);
		switch (s)
		  {
		  case XK_Alt_L:     nf = 1<<0; break;
		  case XK_Meta_L:    nf = 1<<0; break;
		  case XK_Alt_R:     nf = 1<<1; break;
		  case XK_Meta_R:    nf = 1<<1; break;
		  case XK_Control_L: nf = 1<<2; break;
		  case XK_Control_R: nf = 1<<3; break;
		  }

		if (event.xany.type == KeyPress)
		  flags |= nf;
		else
		  flags &= ~nf;
		win = (flags == 15 || flags == 7 || flags == 11);
		if (nf && !win)
		  event.xany.type = 0;
	      }

	    if (!timed_out &&
		(plonkp_cancel ||
                 event.xany.type == KeyPress ||
		 event.xany.type == ButtonPress))
	      {
		XtRemoveTimeOut (id);
		timed_out = True;
		id = 0;
	      }
	    if (event.xany.type)
	      XtDispatchEvent (&event);
	    pending = XtAppPending (fe_XtAppContext);
	  }
	while ((pending & XtIMXEvent) && !timed_out);

      if ((pending & (~XtIMXEvent)) && !timed_out)
	do
	  {
	    XtAppProcessEvent (fe_XtAppContext, (~XtIMXEvent));
	    pending = XtAppPending (fe_XtAppContext);
	  }
	while ((pending & (~XtIMXEvent)) && !timed_out);

      /* If netlib is anxious, service it after every event, and service it
	 continuously while there are no events. */
      if (fe_netlib_hungry_p && !timed_out)
	do
	  {
#ifdef QUANTIFY
quantify_start_recording_data();
#endif /* QUANTIFY */
	    NET_ProcessNet (-1, NET_EVERYTIME_TYPE);
#ifdef QUANTIFY
quantify_stop_recording_data();
#endif /* QUANTIFY */
	  }
	while (fe_netlib_hungry_p && !XtAppPending (fe_XtAppContext)
	       && !timed_out);
    }

  if (sploosh) {
    free (sploosh);
    sploosh = 0;
  }

  /* We timed out. But the context might have been destroyed by now
     if the user killed it with the window manager WM_DELETE. So we
     validate the context. */
  if (!fe_contextIsValid(context)) return True;

  if (SHIST_GetCurrent (&context->hist))
    /* If there's a current URL, then the remote control code must have
       kicked in already.  Don't clear it all. */
    return 1;
  
  if (plonkp_cancel)
      /* if the application forced a cancellation, don't load sploosh */
      return 1;
  
  sploosh = strdup (win ? au : blank);
  for (tmp = sploosh; *tmp; tmp++) *tmp -= 69;
  fe_GetURL (context, NET_CreateURLStruct (sploosh, NET_DONT_RELOAD), FALSE);
  return win;
}


/* #define NEW_VERSION_CHECKING */


#ifdef NEW_VERSION_CHECKING
static Bool fe_version_checker (XrmDatabase *db,
				XrmBindingList bindings,
				XrmQuarkList quarks,
				XrmRepresentation *type,
				XrmValue *value,
				XPointer closure);
#endif /* NEW_VERSION_CHECKING */

static void
fe_sanity_check_resources (Widget toplevel)
{
  XrmDatabase db = XtDatabase (XtDisplay (toplevel));
  XrmValue value;
  char *type;
  char full_name [2048], full_class [2048];
  int count;
  PR_snprintf (full_name, sizeof (full_name), "%s.version", fe_progname);
  PR_snprintf (full_class, sizeof (full_class), "%s.version", fe_progclass);

#ifdef NEW_VERSION_CHECKING

  if (XrmGetResource (db, full_name, full_class, &type, &value) == True)
    {
      /* We have found the resource "Netscape.version: N.N".
	 Versions past 1.1b3 does not use that, so that must
	 mean an older resource db is being picked up.
	 Complain, and refuse to run until this is fixed.
       */
      char str [1024];
      strncpy (str, (char *) value.addr, value.size);
      str [value.size] = 0;

#define VERSION_COMPLAINT_FORMAT XP_GetString( XFE_VERSION_COMPLAINT_FORMAT )

      fprintf (stderr, VERSION_COMPLAINT_FORMAT,
	       fe_progname, fe_version, str, fe_progclass);
      exit (-1);
    }

  {
    const char *s1;
    char *s2;
    XrmName name_list [100];
    XrmClass class_list [100];
    char clean_version [255];

    /* Set clean_version to be a quark-safe version of fe_version,
       ie, "1.1b3N" ==> "v11b3N". */
    s2 = clean_version;
    *s2++ = 'v';
    for (s1 = fe_version; *s1; s1++)
      if (*s1 != ' ' && *s1 != '\t' && *s1 != '.' && *s1 != '*' && !s1 != '?')
	*s2++ = *s1;
    *s2 = 0;

    XrmStringToNameList (full_name, name_list);
    XrmStringToClassList (full_class, class_list);

    XrmEnumerateDatabase (db, name_list, class_list, XrmEnumOneLevel,
			  fe_version_checker, (XtPointer) clean_version);

    /* If we've made it to here, then our iteration over the database
       has not turned up any suspicious "version" resources.  So now
       look for the version tag to see if we have any resources at all.
     */
    PR_snprintf (full_name, sizeof (full_name), 
		    "%s.version.%s", fe_progname, clean_version);
    PR_snprintf (full_class, sizeof (full_class),
		    "%s.version.%s", fe_progclass, clean_version);
    XrmStringToNameList (full_name, name_list);
    XrmStringToClassList (full_class, class_list);
    if (XrmGetResource (db, full_name, full_class, &type, &value) != True)
      {
	/* We have not found "Netscape.version.WRONG: WRONG", but neither
	   have we found "Netscape.version.RIGHT: RIGHT".  Die.
	 */
	fprintf (stderr, XP_GetString( XFE_INAPPROPRIATE_APPDEFAULT_FILE ),
	       fe_progname, fe_progclass);
	exit (-1);
      }
  }

#else /* !NEW_VERSION_CHECKING */

  if (XrmGetResource (db, full_name, full_class, &type, &value) == True)
    {
      char str [1024];
      strncpy (str, (char *) value.addr, value.size);
      str [value.size] = 0;
      if (strcmp (str, fe_version))
	{
	  fprintf (stderr,
			   XP_GetString(XFE_MOZILLA_WRONG_RESOURCE_FILE_VERSION),
			   fe_progname, fe_version, str, fe_progclass);
	  exit (-1);
	}
    }
  else
    {
      fprintf (stderr,
			   XP_GetString(XFE_MOZILLA_CANNOT_FOND_RESOURCE_FILE),
			   fe_progname, fe_progclass);

      exit (-1);
    }
#endif /* !NEW_VERSION_CHECKING */

  /* Now sanity-check the XKeysymDB. */
  count = 0;
  if (XStringToKeysym ("osfActivate")  == 0) count++;	/* 1 */
  if (XStringToKeysym ("osfCancel")    == 0) count++;	/* 2 */
  if (XStringToKeysym ("osfPageLeft")  == 0) count++;	/* 3 */
  if (XStringToKeysym ("osfLeft")      == 0) count++;	/* 4 */
  if (XStringToKeysym ("osfPageUp")    == 0) count++;	/* 5 */
  if (XStringToKeysym ("osfUp")        == 0) count++;	/* 6 */
  if (XStringToKeysym ("osfBackSpace") == 0) count++;	/* 7 */
  if (count > 0)
    {
		char *str;

		if ( count == 7 )
			str = XP_GetString( XFE_THE_MOTIF_KEYSYMS_NOT_DEFINED );
		else
			str = XP_GetString( XFE_SOME_MOTIF_KEYSYMS_NOT_DEFINED );

      fprintf (stderr, str, fe_progname, XP_AppName );
      /* Don't exit for this one, just limp along. */
    }
}


#ifdef NEW_VERSION_CHECKING
static Bool
fe_version_checker (XrmDatabase *db,
		    XrmBindingList bindings,
		    XrmQuarkList quarks,
		    XrmRepresentation *type,
		    XrmValue *value,
		    XPointer closure)
{
  char *name [100];
  int i = 0;
  char *desired_tail = (char *) closure;
  assert (quarks);
  assert (desired_tail);
  if (!quarks || !desired_tail)
    return False;
  while (quarks[i])
    name [i++] = XrmQuarkToString (quarks[i]);

  if (i != 3 ||
      !!strcmp (name[0], fe_progclass) ||
      !!strcmp (name[1], "version") ||
      name[2][0] != 'v')
    /* this is junk... */
    return False;

  if (value->addr &&
      !strncmp (fe_version, (char *) value->addr, value->size) &&
      i > 0 && !strcmp (desired_tail, name [i-1]))
    {
      /* The resource we have found is the one we want/expect.  That's ok. */
    }
  else
    {
      /* complain. */
      char str [1024];
      strncpy (str, (char *) value->addr, value->size);
      str [value->size] = 0;
      fprintf (stderr, VERSION_COMPLAINT_FORMAT,
	       fe_progname, fe_version, str, fe_progclass);
      exit (-1);
    }

  /* Always return false, to continue iterating. */
  return False;
}
#endif /* NEW_VERSION_CHECKING */




#ifdef __linux
 /* On Linux, Xt seems to prints 
       "Warning: locale not supported by C library, locale unchanged"
    no matter what $LANG is set to, even if it's "C" or undefined.

    This is because `setlocale(LC_ALL, "")' always returns 0, while
    `setlocale(LC_ALL, NULL)' returns "C".  I think the former is
    incorrect behavior, even if the intent is to only support one
    locale, but that's how it is.

    The Linux 1.1.59 man page for setlocale(3) says

       At the moment, the only supported locales are the portable
       "C" and "POSIX" locales, which makes  calling  setlocale()
       almost  useless.	Support for other locales will be there
       Real Soon Now.

    Yeah, whatever.  So until there is real locale support, we might
    as well not try and initialize Xlib to use it.

    The reason that defining `BROKEN_I18N' does not stub out all of
    `lose_internationally()' is that we still need to cope with
    `BROKEN_I18N'.  By calling `setlocale (LC_CTYPE, NULL)' we can
    tell whether the "C" locale is defined, and therefore whether
    the `nls' dir exists, and therefore whether Xlib will dump core
    the first time the user tries to paste.
 */
# define BROKEN_I18N
# define NLS_LOSSAGE
#endif /* __linux */

#if defined(__sun) && !defined(__svr4__)	/* SunOS 4.1.3 */
 /* All R5-based systems have the problem that if the `nls' directory
    doesn't exist, trying to cut or paste causes a core dump in Xlib.
    This controls whether we try to generate a warning about this.
  */
# define NLS_LOSSAGE
#endif /* SunOS 4.1.3 */

#ifdef __386BSD__				/* BSDI is also R5 */
# define NLS_LOSSAGE
#endif /* BSDI */

static void
lose_internationally (void)
{
  char *locale;
  Boolean losing, was_default;

/*
 * For whatever reason, Japanese titles don't work unless you call
 * XtSetLanguageProc on DEC. We call XtAppInitialize later, which
 * calls setlocale. (See the XtAppInitialize call for more
 * comments.) -- erik
 */
#ifdef __osf__
  XtSetLanguageProc(NULL, NULL, NULL);
#else /* __osf__ */
  setlocale (LC_CTYPE, "");
  setlocale (LC_TIME,  "");
  setlocale (LC_COLLATE, "");
  fe_InitCollation();
#endif /* __osf__ */

  losing = !XSupportsLocale();
  locale = setlocale (LC_CTYPE, NULL);
  if (! locale) locale = "C";

  was_default = !strcmp ("C", locale);

  if (losing && !was_default)
    {
      fprintf (stderr,
			   XP_GetString(XFE_MOZILLA_LOCALE_NOT_SUPPORTED_BY_XLIB),
			   fe_progname, locale);

      setlocale (LC_CTYPE, "C");
      putenv ("LC_CTYPE=C");
      losing = !XSupportsLocale();
    }

  if (losing)
  {
      fprintf (stderr, 
			   (was_default ?
				XP_GetString(XFE_MOZILLA_LOCALE_C_NOT_SUPPORTED) :
				XP_GetString(XFE_MOZILLA_LOCALE_C_NOT_SUPPORTED_EITHER)),
			   fe_progname);


#ifdef NLS_LOSSAGE
	  {
		  char buf[64];

# if defined (__linux)
# if defined (MKLINUX)
		  XP_SPRINTF(buf,"/usr/X11/lib/X11/locale/");
# else
		  XP_SPRINTF(buf,"/usr/X386/lib/X11/nls/");
# endif
# elif defined (__386BSD__)
		  XP_SPRINTF(buf,"/usr/X11/lib/X11/nls/");
# else /* SunOS 4.1.3, and a guess at anything else... */
		  XP_SPRINTF(buf,"/usr/lib/X11/nls/");
# endif

		  fprintf(stderr,
				  XP_GetString(XFE_MOZILLA_NLS_LOSAGE),
				  XP_AppName, 
				  XP_AppName,
				  buf);
	  }
#else /* !NLS_LOSSAGE */

		  fprintf(stderr,XP_GetString(XFE_MOZILLA_NLS_PATH_NOT_SET_CORRECTLY));

#endif /* !NLS_LOSSAGE */
  }

#ifdef BROKEN_I18N
  losing = True;
#endif /* BROKEN_I18N */

  /* If we're in a losing state, calling these will cause Xlib to exit.
     It's arguably better to try and limp along. */
  if (! losing)
    {
      /*
      XSetLocaleModifiers (NULL);
      */
      XSetLocaleModifiers ("");
      /*
      XtSetLanguageProc (NULL, NULL, NULL);
      */
    }
}


static char *fe_accept_language = NULL;

void
fe_SetAcceptLanguage(char *new_accept_lang)
{
  if (fe_accept_language)
    XP_FREE(fe_accept_language);

  fe_accept_language = new_accept_lang;
}


char *
FE_GetAcceptLanguage(void)
{
	return fe_accept_language;
}


static void
fe_fix_fds (void)
{
  /* Bad Things Happen if stdin, stdout, and stderr have been closed
     (as by the `sh' incantation "netscape 1>&- 2>&-").  I think what
     happens is, the X connection gets allocated to one of these fds,
     and then some random library writes to stderr, and the connection
     gets hosed.  I think this must be a general problem with X programs,
     but I'm not sure.  Anyway, cause them to be open to /dev/null if
     they aren't closed.  This must be done before any other files are
     opened.

     We do this by opening /dev/null three times, and then closing those
     fds, *unless* any of them got allocated as #0, #1, or #2, in which
     case we leave them open.  Gag.
   */
  int fd0 = open ("/dev/null", O_RDWR);
  int fd1 = open ("/dev/null", O_RDWR);
  int fd2 = open ("/dev/null", O_RDWR);
  if (fd0 > 2U) close (fd0);
  if (fd1 > 2U) close (fd1);
  if (fd2 > 2U) close (fd2);
}


extern char **environ;

static void
fe_fix_environment (Display *dpy)
{
  char buf [1024];
  static Boolean env_hacked_p = False;

  if (env_hacked_p)
    return;
  env_hacked_p = True;

  /* Store $DISPLAY into the environment, so that the $DISPLAY variable that
     the spawned processes inherit is the same as the value of -display passed
     in on our command line (which is not necessarily the same as what our
     $DISPLAY variable is.)
   */
  PR_snprintf (buf, sizeof (buf), "DISPLAY=%.900s", DisplayString (dpy));
  if (putenv (strdup (buf)))
    abort ();
}

static void
fe_hack_uid(void)
{
  /* If we've been run as setuid or setgid to someone else (most likely root)
     turn off the extra permissions.  Nobody ought to be installing Mozilla
     as setuid in the first place, but let's be extra special careful...
     Someone might get stupid because of movemail or something.
  */
  setgid (getgid ());
  setuid (getuid ());

  /* Is there anything special we should do if running as root?
     Features we should disable...?
     if (getuid () == 0) ...
   */
}


static void fe_event_processor_callback (XtPointer closure, int *fd, XtIntervalId *id)
{
    XP_ASSERT(*fd == PR_GetEventQueueSelectFD(mozilla_event_queue));
    PR_ProcessPendingEvents(mozilla_event_queue);
}

#ifdef JAVA
void FE_TrackJavaConsole(int on, void *notused)
{
    struct fe_MWContext_cons *cl = fe_all_MWContexts;
    Widget widget;
    while (cl) {
	MWContext *cx = cl->context;
	CONTEXT_DATA(cx)->show_java_console_p = on;
	if ((widget = CONTEXT_DATA(cx)->show_java) != NULL) {
	    XtVaSetValues(widget, XmNset, on, 0);
	}
	cl = cl->next;
    }
}
#endif /* JAVA */

/* Dummy input precedures and timer callbacks to track Xt's select fd
** in nspr
*/
static void dummyInputProc(XtPointer closure, int *fd, XtInputId *id)
{
}

#ifdef XFE_XLOCK_FD_TIMER_HACK
/* see comments below */
static void dummyTimerProc(XtPointer closure, XtIntervalId *id)
{
	(void) XtAppAddTimeOut(fe_XtAppContext, 500L, dummyTimerProc, NULL);
}
#endif /*XFE_XLOCK_FD_TIMER_HACK*/

static void fe_late_init(void)
{
  LOCK_FDSET();
  XtAppAddInput (fe_XtAppContext, PR_GetEventQueueSelectFD(mozilla_event_queue),
		 (XtPointer) (XtInputReadMask),
		 fe_event_processor_callback, 0);
  UNLOCK_FDSET();
}

static int
fe_xt_hack_okayToReleaseXLock()
{
  return(XEventsQueued(fe_display, QueuedAlready) == 0);
}

static Boolean fe_command_line_done = False;

Display *fe_dpy_kludge;
Screen *fe_screen_kludge;

static char *fe_home_dir;
static char *fe_config_dir;

/*
 * build_simple_user_agent_string
 */
static void
build_simple_user_agent_string(char *versionLocale)
{
    char buf [1024];
    int totlen;

    if (XP_AppVersion) {
	free((void *)XP_AppVersion);
	XP_AppVersion = NULL;
    }

    /* SECNAV_Init must have a language string in XP_AppVersion.
     * If missing, it is supplied here.  This string is very short lived.
     * After SECNAV_Init returns, build_user_agent_string() will build the 
     * string that is expected by existing servers.  
     */
    if (!versionLocale || !*versionLocale) {	
	versionLocale = " [en]";   /* default is english for SECNAV_Init() */
    }

    /* be safe about running past end of buf */
    totlen = sizeof buf;
    memset(buf, 0, totlen);
    strncpy(buf, fe_version, totlen - 1);
    totlen -= strlen(buf);
    strncat(buf, versionLocale, totlen - 1);

    XP_AppVersion = strdup (buf);
    /* if it fails, leave XP_AppVersion NULL */
}

/*
 * build_user_agent_string
 */
static void
build_user_agent_string(char *versionLocale)
{
    char buf [1024];
    struct utsname uts;

#ifndef __sgi
    if (fe_VendorAnim) abort (); /* only SGI has one of these now. */
#endif /* __sgi */

    strcpy (buf, fe_version);

#ifdef GOLD
    strcat(buf, "Gold");
#endif

    if ( ekit_Enabled() && ekit_UserAgent() ) { 
        strcat(buf, "C-");
        strcat(buf, ekit_UserAgent());
    }

    if (versionLocale) {
	strcat (buf, versionLocale);
    }

    strcat (buf, " (X11; ");
    /* US or International security versions */
    strcat (buf, SECNAV_SecurityVersion (PR_FALSE));
    strcat (buf, "; ");

    if (uname (&uts) < 0)
      {
#if defined(__sun)
	strcat (buf, "SunOS");
#elif defined(__sgi)
	strcat (buf, "IRIX");
#elif defined(__FreeBSD__)
	strcat (buf, "FreeBSD");
#elif defined(__386BSD__)
	strcat (buf, "BSD/386");
#elif defined(__osf__)
	strcat (buf, "OSF1");
#elif defined(AIXV3)
	strcat (buf, "AIX");
#elif defined(_HPUX_SOURCE)
	strcat (buf, "HP-UX");
#elif defined(__linux)
#if defined(MKLINUX)
	strcat (buf, "MkLinux");
#else
	strcat (buf, "Linux");
#endif
#elif defined(_ATT4)
	strcat (buf, "NCR/ATT");
#elif defined(__USLC__)
	strcat (buf, "UNIX_SV");
#elif defined(sony)
	strcat (buf, "NEWS-OS");
#elif defined(nec_ews)
	strcat (buf, "NEC/EWS-UX/V");
#elif defined(SNI)
	strcat (buf, "SINIX-N");
#else
	ERROR!! run "uname -s" and put the result here.
#endif
        strcat (buf,XP_GetString(XFE_MOZILLA_UNAME_FAILED));

	fprintf (real_stderr,
			 XP_GetString(XFE_MOZILLA_UNAME_FAILED_CANT_DETERMINE_SYSTEM),
			 fe_progname);
      }
    else
      {
	char *s;

#if defined(_ATT4)
	strcpy(uts.sysname,"NCR/ATT");
#endif   /* NCR/ATT */

#if defined(nec_ews)
        strcpy(uts.sysname,"NEC");
#endif   /* NEC */


	PR_snprintf (buf + strlen(buf), sizeof(buf) - strlen(buf),
#ifdef AIX
			/* AIX uname is incompatible with the rest of the Unix world. */
			"%.100s %.100s.%.100s", uts.sysname, uts.version, uts.release);
#else
			"%.100s %.100s", uts.sysname, uts.release);
#endif

	/* If uts.machine contains only hex digits, throw it away.
	   This is so that every single AIX machine in
	   the world doesn't generate a unique vendor ID string. */
	s = uts.machine;
	while (s && *s)
	  {
	    if (!isxdigit (*s))
	      break;
	    s++;
	  }
	if (s && *s)
	  PR_snprintf (buf + strlen (buf), sizeof(buf)-strlen(buf),
			" %.100s", uts.machine);
      }

#ifndef MOZ_COMMUNICATOR_NAME
    strcat (buf, "; Nav");
#endif

    strcat (buf, ")");


#if defined(__sun) && !defined(__svr4__)	/* compiled on SunOS 4.1.3 */
    if (uts.release [0] == '5')
      {

	fprintf (stderr,
			 XP_GetString(XFE_MOZILLA_TRYING_TO_RUN_SUNOS_ON_SOLARIS),
			 fe_progname, 
			 uts.release);
      }
#endif /* 4.1.3 */

    assert(strlen(buf) < sizeof buf);
    if (strlen(buf) >= sizeof(buf)) abort(); /* stack already damaged */

    if (XP_AppVersion) {
	free((void *)XP_AppVersion);
	XP_AppVersion = NULL;
    }
    XP_AppVersion = strdup (buf);
    if (!XP_AppVersion) {
	XP_AppVersion = "";
    }
}

/*
 *    Sorry folks, C++ has come to town. *Some* C++ compilers/linkers require
 *    the program's main() to be in a C++ file. I've added a losing C++ main()
 *    in cplusplusmain.cc. This guy will call mozilla_main(). Look, I'm really
 *    sorry about this. I don't know what to say. C++ loses, Cfront loses,
 *    we are now steeped in all of their lossage. We will descend into the
 *    burning pit of punctuation used for obscure language features, our
 *    links will go asunder, when we seek out our proper inheritance, from
 *    the mother *and* father from which we were begat, we shall be awash,
 *    for we will be without identity. We shall try to be pure, but shall find
 *    ourselves unable to instanciate. ... shall be statically bound unto
 *    him. Oh, woe unto us... djw
 */
int
#ifdef CPLUSPLUS_LINKAGE
mozilla_main
#else
main
#endif
(int argc, char** argv)
{
  Widget toplevel;
  Display *dpy;
  int i;
  Widget disp_obj;
  struct sigaction act;

  char *s;
  char **remote_commands = 0;
  int remote_command_count = 0;
  int remote_command_size = 0;
  unsigned long remote_window = 0;

  XP_StatStruct stat_struct;

#ifdef MOZ_MAIL_NEWS
  MWContext* biffcontext;
#endif

  XP_Bool netcasterShown=FALSE;

  char versionLocale[32];

  versionLocale[0] = 0;

  fe_progname = argv [0];
  s = XP_STRRCHR (fe_progname, '/');
  if (s) fe_progname = s + 1;

  real_stderr = stderr;
  real_stdout = stdout;

  fe_hack_uid();	/* Do this real early */

  /* 
   * Check the environment for MOZILLA_NO_ASYNC_DNS.  It would be nice to
   * make this either a pref or a resource/option.  But, at this point in
   * the game neither prefs nor xt resources have been initialized and read.
   */
  fe_check_use_async_dns();

#ifdef UNIX_ASYNC_DNS
  if (fe_UseAsyncDNS())
  {
	  XFE_InitDNS_Early(argc,argv);   /* Do this early (before Java, NSPR.) */
  }
#endif

#ifdef MOZILLA_GPROF
   /*
    *    Do this early, but after the async DNS process has spawned.
    */
   gmon_init(); /* initialize, maybe start, profiling */
#endif /*MOZILLA_GPROF*/

  /*
  ** Initialize the runtime, preparing for multi-threading. Make mozilla
  ** a higher priority than any java applet. Java's priority range is
  ** 1-10, and we're mapping that to 11-20 (in sysThreadSetPriority).
  */
#ifdef NSPR20
/* No explicit initialization needed for NSPR 2.0 */
  {
#ifdef DEBUG
      extern PRLogModuleInfo* NETLIB;
#endif

#ifdef MOZ_MAIL_NEWS
      extern PRLogModuleInfo* NNTP;
#endif

#ifdef DEBUG
      NETLIB = PR_NewLogModule("netlib");
#endif

#ifdef MOZ_MAIL_NEWS
      NNTP = PR_NewLogModule("nntp");
#endif

      PR_SetThreadGCAble();
	  PR_SetThreadPriority(PR_GetCurrentThread(), PR_PRIORITY_LAST);
	  PR_BlockClockInterrupts();
  }

#elif defined(NSPR_SPLASH) /* !NSPR20 && NSPR_SPLASH */
  /*
  ** if we're using the splash screen, we need the mozilla thread to
  ** be at a lower priority (so the label gets updated and we handle X
  ** events quickly.  It may make the startup time longer, but the user
  ** will be happier.  We'll set it back after fe_splashStop().
  */
  PR_Init("mozilla", 20, 1, 0);
#else                      /* !NSPR20 && !NSPR_SPLASH */
  PR_Init("mozilla", 24, 1, 0);
#endif                     /* NSRP20 */

  LJ_SetProgramName(argv[0]);
  PR_XLock();
  mozilla_thread = PR_CurrentThread();
  fdset_lock = PR_NewNamedMonitor("mozilla-fdset-lock");

  /*
  ** Create a pipe used to wakeup mozilla from select. A problem we had
  ** to solve is the case where a non-mozilla thread uses the netlib to
  ** fetch a file. Because the netlib operates by updating mozilla's
  ** select set, and because mozilla might be in the middle of select
  ** when the update occurs, sometimes mozilla never wakes up (i.e. it
  ** looks hung). Because of this problem we create a pipe and when a
  ** non-mozilla thread wants to wakeup mozilla we write a byte to the
  ** pipe.
  */
  mozilla_event_queue = PR_CreateEventQueue("mozilla-event-queue", mozilla_thread);

  if (mozilla_event_queue == NULL) 
  {
      fprintf(stderr,
			  XP_GetString(XFE_MOZILLA_FAILED_TO_INITIALIZE_EVENT_QUEUE), 
			  fe_progname);
	  
      exit(-1);
  }

  fe_fix_fds ();

#ifdef MEMORY_DEBUG
#ifdef __sgi
  mallopt (M_DEBUG, 1);
  mallopt (M_CLRONFREE, 0xDEADBEEF);
#endif
#endif

#ifdef DEBUG
      {
	  extern int il_debug;
	  if(getenv("ILD"))
	      il_debug=atoi(getenv("ILD"));
      }

  if(stat(".WWWtraceon", &stat_struct) != -1)
      MKLib_trace_flag = 2;
#endif

  /* user agent stuff used to be here */
  fe_progclass = XFE_PROGCLASS_STRING;
  XP_AppName = fe_progclass;
  XP_AppCodeName = "Mozilla";

  /* OSTYPE is defined by the build environment (config.mk) */
  XP_AppPlatform = OSTYPE;


  save_argv = (char **)calloc(argc, sizeof(char*));

  save_argc = 1;
  /* Hack the -help and -version arguments before opening the display. */
  for (i = 1; i < argc; i++)
    {
      if (!XP_STRCASECMP(argv [i], "-h") ||
	  !XP_STRCASECMP(argv [i], "-help") ||
	  !XP_STRCASECMP(argv [i], "--help"))
	{
	  usage ();
	  exit (0);
	}
/* because of stand alone java, we need not support -java. sudu/warren */
#ifdef JAVA
#ifdef JAVA_COMMAND_LINE_SUPPORT
/*
 * -java will not be supported once it has been replaced by stand alone java
 * this ifdef remains in case someone needs it and should be removed once
 * stand alone java is in the mainstream sudu/warren
 */

      else if (!XP_STRCASECMP(argv [i], "-java") ||
	       !XP_STRCASECMP(argv [i], "--java"))
	{
	  extern int start_java(int argc, char **argv);
	  start_java(argc-i, &argv[i]);
	  exit (0);
	}
#endif /* JAVA_COMMAND_LINE_SUPPORT */
#endif
      else if (!XP_STRCASECMP(argv [i], "-v") ||
	       !XP_STRCASECMP(argv [i], "-version") ||
	       !XP_STRCASECMP(argv [i], "--version"))
	{
	  fprintf (stderr, "%s\n", fe_long_version + 4);
	  exit (0);
	}
      else if (!XP_STRCASECMP(argv [i], "-remote") ||
	       !XP_STRCASECMP(argv [i], "--remote"))
	{
	  if (remote_command_count == remote_command_size)
	    {
	      remote_command_size += 20;
	      remote_commands =
		(remote_commands
		 ? realloc (remote_commands,
			    remote_command_size * sizeof (char *))
		 : calloc (remote_command_size, sizeof (char *)));
	    }
	  i++;
	  if (!argv[i] || *argv[i] == '-' || *argv[i] == 0)
	    {
	      fprintf (stderr,
				   XP_GetString(XFE_MOZILLA_INVALID_REMOTE_OPTION),
				   fe_progname, 
				   argv[i] ? argv[i] : "");
	      usage ();
	      exit (-1);
	    }
	  remote_commands [remote_command_count++] = argv[i];
	}
      else if (!XP_STRCASECMP(argv [i], "-raise") ||
	       !XP_STRCASECMP(argv [i], "--raise") ||
	       !XP_STRCASECMP(argv [i], "-noraise") ||
	       !XP_STRCASECMP(argv [i], "--noraise"))
	{
	  char *r = argv [i];
	  if (r[0] == '-' && r[1] == '-')
	    r++;
	  if (remote_command_count == remote_command_size)
	    {
	      remote_command_size += 20;
	      remote_commands =
		(remote_commands
		 ? realloc (remote_commands,
			    remote_command_size * sizeof (char *))
		 : calloc (remote_command_size, sizeof (char *)));
	    }
	  remote_commands [remote_command_count++] = r;
	}
      else if (!XP_STRCASECMP(argv [i], "-netcaster") ||
	       !XP_STRCASECMP(argv [i], "--netcaster")
	       )
	{
	  char *r = argv [i];
	  if (r[0] == '-' && r[1] == '-')
	    r++;
	  fe_globalData.startup_netcaster=True;
	}
      else if (!XP_STRCASECMP(argv [i], "-id") ||
	       !XP_STRCASECMP(argv [i], "--id"))
	{
	  char c;
	  if (remote_command_count > 0)
	    {
			fprintf (stderr,
					 XP_GetString(XFE_MOZILLA_ID_OPTION_MUST_PRECEED_REMOTE_OPTIONS),
		       fe_progname);
	      usage ();
	      exit (-1);
	    }
	  else if (remote_window != 0)
	    {
	      fprintf (stderr,
				   XP_GetString(XFE_MOZILLA_ONLY_ONE_ID_OPTION_CAN_BE_USED),
				   fe_progname);
	      usage ();
	      exit (-1);
	    }
	  i++;
	  if (argv[i] &&
	      1 == sscanf (argv[i], " %ld %c", &remote_window, &c))
	    ;
	  else if (argv[i] &&
		   1 == sscanf (argv[i], " 0x%lx %c", &remote_window, &c))
	    ;
	  else
	    {
	      fprintf (stderr,
				   XP_GetString(XFE_MOZILLA_INVALID_OD_OPTION),
				   fe_progname, argv[i] ? argv[i] : "");
	      usage ();
	      exit (-1);
	    }
	 }
       else  /* Save these args  for session manager */
         {
	     save_argv[save_argc] = 0;
	     save_argv[save_argc] = (char*)malloc((strlen(argv[i])+1)*
				sizeof(char*)); 
             strcpy(save_argv[save_argc], argv[i]);
	     save_argc++;
         }
     }

  lose_internationally ();

#if defined(__sun)
  {
    /* The nightmare which is XKeysymDB is unrelenting. */
    /* It'd be nice to check the existence of the osf keysyms here
       to know whether we're screwed yet, but we must set $XKEYSYMDB
       before initializing the connection; and we can't query the
       keysym DB until after initializing the connection. */
    char *override = getenv ("XKEYSYMDB");
    struct stat st;
    if (override && !stat (override, &st))
      {
	/* They set it, and it exists.  Good. */
      }
    else
      {
	char *try1 = "/usr/lib/X11/XKeysymDB";
	char *try2 = "/usr/openwin/lib/XKeysymDB";
	char *try3 = "/usr/openwin/lib/X11/XKeysymDB";
	char buf [1024];
	if (override)
	  {
	    fprintf (stderr,
				 XP_GetString(XFE_MOZILLA_XKEYSYMDB_SET_BUT_DOESNT_EXIST),
				 argv [0], override);
	  }

	if (!stat (try1, &st))
	  {
	    PR_snprintf (buf, sizeof (buf), "XKEYSYMDB=%.900s", try1);
	    putenv (buf);
	  }
	else if (!stat (try2, &st))
	  {
	    PR_snprintf (buf, sizeof (buf), "XKEYSYMDB=%.900s", try2);
	    putenv (buf);
	  }
	else if (!stat (try3, &st))
	  {
	    PR_snprintf (buf, sizeof (buf), "XKEYSYMDB=%.900s", try3);
	    putenv (buf);
	  }
	else
	  {
	    fprintf (stderr,
				 XP_GetString(XFE_MOZILLA_NO_XKEYSYMDB_FILE_FOUND),
				 argv [0], try1, try2, try3);
	  }
      }
  }
#endif /* sun */

  /* Must do this early now so that mail can load directories */
  /* Setting fe_home_dir likewise must happen early. */
  fe_home_dir = getenv ("HOME");
  if (!fe_home_dir || !*fe_home_dir)
    {
      /* Default to "/" in case a root shell is running in dire straits. */
      struct passwd *pw = getpwuid(getuid());

      fe_home_dir = pw ? pw->pw_dir : "/";
    }
  else
    {
      char *slash;

      /* Trim trailing slashes just to be fussy. */
      while ((slash = strrchr(fe_home_dir, '/')) && slash[1] == '\0')
	*slash = '\0';
    }
{
    char buf [1024];
    PR_snprintf (buf, sizeof (buf), "%s/%s", fe_home_dir,
#ifdef OLD_UNIX_FILES
        ".netscape-preferences"
#else
        ".netscape/preferences.js"
#endif
		);

    fe_globalData.user_prefs_file = strdup (buf);

    PREF_Init((char*) fe_globalData.user_prefs_file);
}

#ifdef MOZ_MAIL_NEWS
  fe_mailNewsPrefs = MSG_CreatePrefs();
#endif

  toplevel = XtAppInitialize (&fe_XtAppContext, (char *) fe_progclass, options,
			      sizeof (options) / sizeof (options [0]),
			      &argc, argv, fe_fallbackResources, 0, 0);



  FE_SetToplevelWidget(toplevel);

  /* we need to set the drag/drop protocol style to dynamic
     or the drag-into-menus stuff for the 4.0 quickfile
     stuff won't work. */
  disp_obj = XmGetXmDisplay(XtDisplay(toplevel));

  XtVaSetValues(disp_obj,
		XmNdragInitiatorProtocolStyle, XmDRAG_PREFER_DYNAMIC,
		XmNdragReceiverProtocolStyle, XmDRAG_PREFER_DYNAMIC,
		NULL);

  fe_progname_long = fe_locate_program_path (argv [0]);

/*
 * We set all locale categories except for LC_CTYPE back to "C" since
 * we want to be careful, and since we already know that libnet depends
 * on the C locale for strftime for HTTP, NNTP and RFC822. We call
 * XtSetLanguageProc before calling XtAppInitialize, which makes the latter
 * call setlocale for all categories. Hence the setting back to "C"
 * stuff here. See other comments near the XtSetLanguageProc call. -- erik
 */
#ifdef __osf__
  /*
   * For some strange reason, we cannot set LC_COLLATE to "C" on DEC,
   * otherwise the text widget misbehaves, e.g. deleting one byte instead
   * of two when backspacing over a Japanese character. Grrr. -- erik
   */
  /* setlocale(LC_COLLATE, "C"); */
  fe_InitCollation();

  setlocale(LC_MONETARY, "C");
  setlocale(LC_NUMERIC, "C");
  /*setlocale(LC_TIME, "C");*/
  setlocale(LC_MESSAGES, "C");
#endif /* __osf__ */

  fe_display = dpy = XtDisplay (toplevel);

#if defined(NSPR20)
  PR_UnblockClockInterrupts();
#else
  /* Now we can turn the nspr clock on */
  PR_StartEvents(0);
#endif	/* NSPR20 */

  {
    extern void AwtRegisterXtAppVars(Display *dpy,
				     XtAppContext ac, char *class);
    AwtRegisterXtAppVars(fe_display, fe_XtAppContext, (char *)fe_progclass);
  }

 {
 	extern int PR_XGetXtHackFD(void);
 	int fd;
 
 	/*
 	 * We would like to yield the X lock to other (Java) threads when
 	 * the Mozilla thread calls select(), but we should only do so
 	 * when Xt calls select from its main event loop. (There are other
 	 * times when select is called, e.g. from within Xlib. At these
 	 * times, it is not safe to release the X lock.) Within select,
 	 * we detect that the caller is Xt by looking at the file
 	 * descriptors. If one of them is this special unused FD that we
 	 * are adding here, then we know that Xt is calling us, so it is
 	 * safe to release the X lock. -- erik
 	 */
 	fd = PR_XGetXtHackFD();
 	if (fd >= 0) {
 		(void) XtAppAddInput(fe_XtAppContext, fd,
 			(XtPointer) XtInputReadMask, dummyInputProc, NULL);
 	}
 
#ifdef XFE_XLOCK_FD_TIMER_HACK
	/*
	 * don't think we need this anymore, and we'd like to be able to go
	 * completely idle....djw & radha
	 */
 	/*
 	 * For some reason, Xt sometimes calls select() only with the FD for
 	 * the X connection, without a timeout. Since we only release the
 	 * X lock when the special FD (added above) is selected, we need to
 	 * have some way of breaking out of this situation. The following
 	 * hack sets a 500 millisecond timer for this purpose. When the timer
 	 * fires, the next time Xt calls select, it will be with the right
 	 * FDs. Ugh. -- erik
 	 */
  	(void) XtAppAddTimeOut(fe_XtAppContext, 500L, dummyTimerProc, NULL); 
#endif /*XFE_XLOCK_FD_TIMER_HACK*/
   }

  /* For security stuff... */
  fe_dpy_kludge = dpy;
  fe_screen_kludge = XtScreen (toplevel);


  /* Initialize the security library. This must be done prior
     to any calls that will cause X event activity, since the
     event loop calls security functions.
   */

  XtGetApplicationNameAndClass (dpy,
				(char **) &fe_progname,
				(char **) &fe_progclass);

  /* Things blow up if argv[0] has a "." in it. */
  {
    char *s = (char *) fe_progname;
    while ((s = strchr (s, '.')))
      *s = '_';
  }


  /* If there were any -command options, then we are being invoked in our
     role as a controller of another Mozilla window; find that other window,
     send it the commands, and exit. */
  if (remote_command_count > 0)
    {
      int status = fe_RemoteCommands (dpy, (Window) remote_window,
				      remote_commands);
      exit (status);
    }
  else if (remote_window)
    {
      fprintf (stderr,
			   XP_GetString(XFE_MOZILLA_ID_OPTION_CAN_ONLY_BE_USED_WITH_REMOTE),
	       fe_progname);
      usage ();
      exit (-1);
    }

  fe_sanity_check_resources (toplevel);

  {
    char	clas[128];
    XrmDatabase	db;
    char	*locale;
    char	name[128];
    char	*type;
    XrmValue	value;

    db = XtDatabase(dpy);
    PR_snprintf(name, sizeof(name), "%s.versionLocale", fe_progname);
    PR_snprintf(clas, sizeof(clas), "%s.VersionLocale", fe_progclass);
    if (XrmGetResource(db, name, clas, &type, &value) && *((char *) value.addr))
      {
        PR_snprintf(versionLocale, sizeof(versionLocale), " [%s]",
		(char *) value.addr);

        XP_AppLanguage = strdup((char*)value.addr);

        fe_version_and_locale = PR_smprintf("%s%s", fe_version, versionLocale);
        if (!fe_version_and_locale)
	  {
	    fe_version_and_locale = (char *) fe_version;
	  }
      }
    if (!XP_AppLanguage) {
	XP_AppLanguage = strdup("en");
    }

    PR_snprintf(name, sizeof(name), "%s.httpAcceptLanguage", fe_progname);
    PR_snprintf(clas, sizeof(clas), "%s.HttpAcceptLanguage", fe_progclass);
    if (XrmGetResource(db, name, clas, &type, &value) && *((char *) value.addr))
      {
	fe_accept_language = (char *) value.addr;
      }

    locale = fe_GetNormalizedLocaleName();
    if (locale)
      {
        PR_snprintf(name, sizeof(name), "%s.localeCharset.%s", fe_progname,
		locale);
	free((void *) locale);
      }
    else
      {
        PR_snprintf(name, sizeof(name), "%s.localeCharset.C", fe_progname);
      }
    PR_snprintf(clas, sizeof(clas), "%s.LocaleCharset.Locale", fe_progclass);
    if (XrmGetResource(db, name, clas, &type, &value) && *((char *) value.addr))
      {
	fe_LocaleCharSetID = INTL_CharSetNameToID((char *) value.addr);
	if (fe_LocaleCharSetID == CS_UNKNOWN)
	  {
	    fe_LocaleCharSetID = CS_LATIN1;
	  }
      }
    else
      {
	fe_LocaleCharSetID = CS_LATIN1;
      }
    INTL_CharSetIDToName(fe_LocaleCharSetID, fe_LocaleCharSetName);
  }

  /* Now that we've got a display and know we're going to behave as a
     client rather than a remote controller, fix the $DISPLAY setting. */
  fe_fix_environment (dpy);

  /* Hack remaining arguments - assume things beginning with "-" are
     misspelled switches, instead of file names.  Magic for -help and
     -version has already happened.
   */
  for (i = 1; i < argc; i++)
    if (*argv [i] == '-')
      {
	fprintf (stderr, XP_GetString( XFE_UNRECOGNISED_OPTION ),
		 fe_progname, argv [i]);
	usage ();
	exit (-1);
      }


  /* Must be called before XFE_ReadPrefs(). */
  fe_InitFonts(dpy);

  /* See if another instance is running.  If so, don't use the .db files */
  if (fe_ensure_config_dir_exists (toplevel))
    {
      char *name;
      unsigned long addr;
      pid_t pid;

      name = PR_smprintf ("%s/lock", fe_config_dir);
      addr = 0;
      pid = 0;
      if (name == NULL)
	/* out of memory -- what to do? */;
      else if (fe_create_pidlock (name, &addr, &pid) == 0)
	{
	  /* Remember name in fe_pidlock so we can unlink it on exit. */
	  fe_pidlock = name;

	  /* Remember that we have access to the databases.
	   * This lets us know when we can enable the history window. */
	  fe_globalData.all_databases_locked = FALSE;
	}
      else 
	{
	  char *fmt = NULL;
	  char *lock = name ? name : ".netscape/lock";

	  fmt = PR_sprintf_append(fmt, XP_GetString(XFE_APP_HAS_DETECTED_LOCK),
				  XP_AppName, lock);
	  fmt = PR_sprintf_append(fmt,
				  XP_GetString(XFE_ANOTHER_USER_IS_RUNNING_APP),
				  XP_AppName,
				  fe_config_dir);

	  if (addr != 0 && pid != 0)
	    {
	      struct in_addr inaddr;
	      PRHostEnt *hp, hpbuf;
	      char dbbuf[PR_NETDB_BUF_SIZE];
	      const char *host;

	      inaddr.s_addr = addr;
#ifndef NSPR20
	      hp = PR_gethostbyaddr((char *)&inaddr, sizeof inaddr, AF_INET,
				    &hpbuf, dbbuf, sizeof(dbbuf), 0);
#else
              {
	      PRStatus sts;
	      PRNetAddr pr_addr;

	      pr_addr.inet.family = AF_INET;
	      pr_addr.inet.ip = addr;
	      sts = PR_GetHostByAddr(&pr_addr,dbbuf, sizeof(dbbuf), &hpbuf);
	      if (sts == PR_FAILURE)
		  hp = NULL;
	      else
		  hp = &hpbuf;
	      }
#endif
	      host = (hp == NULL) ? inet_ntoa(inaddr) : hp->h_name;
	      fmt = PR_sprintf_append(fmt,
		      XP_GetString(XFE_APPEARS_TO_BE_RUNNING_ON_HOST_UNDER_PID),
		      host, (unsigned)pid);
	    }

	  fmt = PR_sprintf_append(fmt,
				  XP_GetString(XFE_YOU_MAY_CONTINUE_TO_USE),
				  XP_AppName);
	  fmt = PR_sprintf_append(fmt,
				  XP_GetString(XFE_OTHERWISE_CHOOSE_CANCEL),
				  XP_AppName, lock, XP_AppName);

	  if (fmt)
	    {
	      if (!fe_Confirm_2 (toplevel, fmt))
		exit (0);
	      free (fmt);
	    }
	  if (name) free (name);

	  /* Keep on-disk databases from being open-able. */
	  dbSetOrClearDBLock (LockOutDatabase);
	  fe_globalData.all_databases_locked = TRUE;
	}
    }

  {
    /* Install the default signal handlers */
    act.sa_handler = fe_sigchild_handler;
    act.sa_flags = 0;
    sigfillset(&act.sa_mask);
    sigaction (SIGCHLD, &act, NULL);

    fe_InitializeGlobalResources (toplevel);

#ifndef OLD_UNIX_FILES

    /* Maybe we want to copy an old set of files before reading them
       with their historically-current names. */
    fe_copy_init_files (toplevel);

#else  /* OLD_UNIX_FILES */

    /* Maybe we want to rename an old set of files before reading them
       with their historically-current names. */
    fe_rename_init_files (toplevel);

#endif /* OLD_UNIX_FILES */

    /* New XP-prefs routine. */
    SECNAV_InitConfigObject();
    
    /* Must be called before XFE_DefaultPrefs */
    ekit_Initialize(toplevel);
	AltMailInit();

    FE_register_pref_callbacks();

    /* Load up the preferences. */
    XFE_DefaultPrefs (&fe_defaultPrefs);

    /* must be called after fe_InitFonts() */
    XFE_ReadPrefs ((char *) fe_globalData.user_prefs_file, &fe_globalPrefs);

	fe_check_prefs_version(&fe_globalPrefs);
	if (fe_globalPrefs.prefs_need_upgrade > 0) {
		if (! fe_CheckVersionAndSavePrefs((char *) fe_globalData.user_prefs_file,
										  &fe_globalPrefs))
			fe_perror_2 (toplevel, XP_GetString( XFE_ERROR_SAVING_OPTIONS ) );
	}	
	else if (fe_globalPrefs.prefs_need_upgrade < 0) {
		char *msg = PR_smprintf(XP_GetString(XFE_PREFS_DOWNGRADE),
								fe_globalPrefs.version_number,
								PREFS_CURRENT_VERSION);
		fe_Alert_2 (toplevel, msg);
		XP_FREE(msg);
	}

{
    char buf [1024];
    PR_snprintf (buf, sizeof (buf), "%s/%s", fe_home_dir, ".netscape/user.js");
    PREF_ReadUserJSFile(buf);
    PR_snprintf (buf, sizeof (buf), "%s/%s", fe_home_dir, ".netscape/hook.js");
    HK_ReadHookFile(buf);
}

    fe_startDisplayFactory(toplevel);

#ifndef OLD_UNIX_FILES
    fe_clean_old_init_files (toplevel); /* finish what we started... */
        /* spider begin */
        /* TODO: need to rename lid cache dir too! */
        /* spider end */
#else  /* OLD_UNIX_FILES */
    /* kludge kludge - the old name is *stored* in the prefs, so if we've
       renamed the cache directory, rename it in the preferences too. */
    if (fe_renamed_cache_dir)
      {
	if (fe_globalPrefs.cache_dir)
	  free (fe_globalPrefs.cache_dir);
	fe_globalPrefs.cache_dir = fe_renamed_cache_dir;
	fe_renamed_cache_dir = 0;
	if (!XFE_SavePrefs ((char *) fe_globalData.user_prefs_file,
			    &fe_globalPrefs))
	  fe_perror_2 (toplevel, XP_GetString( XFE_ERROR_SAVING_OPTIONS ) );
      }
#endif /* OLD_UNIX_FILES */

    GH_SetGlobalHistoryTimeout (fe_globalPrefs.global_history_expiration
				* 24 * 60 * 60);
  }
#ifdef NSPR_SPLASH
    if (fe_globalData.show_splash)
      {
	PR_XUnlock();
	fe_splashStart(toplevel);
      }
#endif

#ifdef NSPR_SPLASH
  if (fe_globalData.show_splash)
    fe_splashUpdateText(XP_GetString(XFE_SPLASH_REGISTERING_CONVERTERS));
#endif

  NR_StartupRegistry();
  fe_RegisterConverters ();  /* this must be before InstallPreferences(),
				and after fe_InitializeGlobalResources(). */

#ifdef SAVE_ALL
  SAVE_Initialize ();	     /* Register some more converters. */
#endif /* SAVE_ALL */

  PREF_SetDefaultCharPref("profile.name", fe_globalPrefs.email_address);
  PREF_SetDefaultCharPref("profile.directory", fe_config_dir);
  PREF_SetDefaultIntPref("profile.numprofiles", 1);

  GH_InitGlobalHistory();

  /* SECNAV_INIT needs this defined, but build_user_agent_string cannot 
   * be called until after SECNAV_INIT, so call this simplified version.
   */
  build_simple_user_agent_string(versionLocale);

  fe_InstallPreferences (0);

  /*
  ** Initialize the security library.
  **
  ** MAKE SURE THIS IS DONE BEFORE YOU CALL THE NET_InitNetLib, CUZ
  ** OTHERWISE THE FILE LOCK ON THE HISTORY DB IS LOST!
  */
#ifdef NSPR_SPLASH
  if (fe_globalData.show_splash)
    fe_splashUpdateText(XP_GetString(XFE_SPLASH_INITIALIZING_SECURITY_LIBRARY));
#endif
  SECNAV_Init();

  /* Must be called after ekit, since user agent is configurable */
  /* Must also be called after SECNAV_Init, since crypto is dynamic */
  build_user_agent_string(versionLocale);

  RNG_SystemInfoForRNG ();
  RNG_FileForRNG (fe_globalPrefs.history_file);
  fe_read_screen_for_rng (dpy, XtScreen (toplevel));
  SECNAV_RunInitialSecConfig();
  
  /* Initialize the network library. */
#ifdef NSPR_SPLASH
  if (fe_globalData.show_splash)
    fe_splashUpdateText(XP_GetString(XFE_SPLASH_INITIALIZING_NETWORK_LIBRARY));
#endif
  /* The unit for tcp buffer size is changed from ktypes to btyes */
  NET_InitNetLib (fe_globalPrefs.network_buffer_size, 50);

#ifdef UNIX_ASYNC_DNS
  if (fe_UseAsyncDNS())
  {
	  /* Finish initializing the nonblocking DNS code. */
	  XFE_InitDNS_Late(fe_XtAppContext);
  }
#endif

  /* Initialize the message library. */
#ifdef NSPR_SPLASH
  if (fe_globalData.show_splash)
    fe_splashUpdateText(XP_GetString(XFE_SPLASH_INITIALIZING_MESSAGE_LIBRARY));
#endif
#ifdef MOZ_MAIL_NEWS
  MSG_InitMsgLib(); 
#endif

  /* Initialize the Image Library */
#ifdef NSPR_SPLASH
  if (fe_globalData.show_splash)
    fe_splashUpdateText(XP_GetString(XFE_SPLASH_INITIALIZING_IMAGE_LIBRARY));
#endif
  IL_Init();

  /* Initialize libmocha after netlib and before plugins. */
#ifdef NSPR_SPLASH
  if (fe_globalData.show_splash)
    fe_splashUpdateText(XP_GetString(XFE_SPLASH_INITIALIZING_MOCHA));
#endif
  LM_InitMocha ();

#ifdef NSPR_SPLASH
  if (fe_globalData.show_splash)
    {
      fe_splashUpdateText(XP_GetString(XFE_SPLASH_INITIALIZING_PLUGINS));
      PR_XLock();
    }
#endif

  NPL_Init();

#ifdef NSPR_SPLASH
  if (fe_globalData.show_splash)
    {
      PR_XUnlock();

      fe_splashStop();

      PR_SetThreadPriority(mozilla_thread, 24);

      PR_XLock();
    }
#endif

#ifdef XFE_RDF
  /* Initialize RDF */
  {
    RDF_InitParamsStruct rdf_params;

    /* we need some initial context, so create bookmarks */
    fe_createBookmarks(toplevel, NULL, NULL);

    rdf_params.profileURL 
      = XP_PlatformFileToURL(fe_config_dir);
    rdf_params.bookmarksURL
      = XP_PlatformFileToURL(fe_globalPrefs.bookmark_file);
    rdf_params.globalHistoryURL
      = XP_PlatformFileToURL(fe_globalPrefs.history_file);

    RDF_Init(&rdf_params);

    XP_FREEIF(rdf_params.profileURL);
    XP_FREEIF(rdf_params.bookmarksURL);
    XP_FREEIF(rdf_params.globalHistoryURL);
  }
#endif /*XFE_RDF*/

  /* Do some late initialization */
  fe_late_init();
#ifdef JAVA
  LJ_SetConsoleShowCallback(FE_TrackJavaConsole, 0);
#endif
  
  fe_InitCommandActions ();
  fe_InitMouseActions ();
  fe_InitKeyActions ();
#ifdef EDITOR
  fe_EditorInitActions();
#endif

#ifdef EDITOR
  fe_EditorStaticInit();
#endif
#ifdef MOZ_MAIL_NEWS
  FE_InitAddrBook();
#endif

  fe_InitPolarisComponents(argv[0]);
  fe_InitConference(argv[0]);

  XSetErrorHandler (x_error_handler);
  XSetIOErrorHandler (x_fatal_error_handler);

  old_xt_warning_handler = XtAppSetWarningMsgHandler(fe_XtAppContext,
						     xt_warning_handler);
  old_xt_warningOnly_handler =  XtAppSetWarningHandler(fe_XtAppContext,
						       xt_warningOnly_handler);

  /* At this point, everything in argv[] is a document for us to display.
     Create a window for each document.
   */
  if (argc > 1)
  {
	  for (i = 1; i < argc; i++)
      {
		  char buf [2048];
		  if (argv [i][0] == '/')
		  {
			  /* It begins with a / so it's a file. */
			  URL_Struct *url;
			  MWContextType type;
			  
			  PR_snprintf (buf, sizeof (buf), "file:%.900s", argv [i]);
			  url = NET_CreateURLStruct (buf, NET_DONT_RELOAD);

#ifdef NON_BETA
			  plonkp = True;
#endif
			  
#ifdef MOZ_MAIL_NEWS
			  if (fe_globalData.startup_composer)
			  {
				  type = MWContextEditor;
			  }
			  else
#endif
			  {
				  type = MWContextBrowser;
			  }
			  
			  fe_MakeWindow (toplevel, 0, url, NULL, type, FALSE);
		  }
		  else if (argv [i][0] == 0)
		  {
			  /* -nethelp takes precedence over -composer */
			  if (fe_globalData.startup_nethelp)
			  {
#ifdef NETHELP_STARTUP_FLAG
				  /* we need some initial context, so create bookmarks */
				  fe_createBookmarks(toplevel, NULL, NULL);
				  /* the bookmarks context gets used in FE_GetNetHelpContext */
				  XP_NetHelp(NULL, HELP_COMMUNICATOR);
#endif
			  }
			  else
			  {
				  MWContextType type;
				  
#ifdef MOZ_MAIL_NEWS
				  if (fe_globalData.startup_composer)
				  {
					  type = MWContextEditor;
				  }
				  else
#endif
				  {
					  type = MWContextBrowser;
				  }
				  
				  fe_MakeWindow (toplevel, 0, 0, NULL, type, FALSE);
			  }
		  }
		  else
		  {
			  PRHostEnt hpbuf;
			  char dbbuf[PR_NETDB_BUF_SIZE];
			  char *s = argv [i];
			  
			  MWContextType type;

#ifdef NETHELP_STARTUP_FLAG
			  /* Has an argument */
			  if (fe_globalData.startup_nethelp)
			  {
				fe_globalData.startup_iconic = TRUE;
			  }
#endif
#ifdef MOZ_MAIL_NEWS
				  if (fe_globalData.startup_composer)
					{
					  type = MWContextEditor;
					}
				  else
#endif
					{
					  type = MWContextBrowser;
					}
			  
				  while (*s && *s != ':' && *s != '/')
					s++;
				  
#ifndef NSPR20
				  if (*s == ':' || PR_gethostbyname (argv [i], &hpbuf, dbbuf, sizeof(dbbuf), 0))
#else
				  if (*s == ':' || (PR_GetHostByName (argv [i], dbbuf, sizeof(dbbuf), &hpbuf) == PR_SUCCESS))
#endif
					{
					  /* There is a : before the first / so it's a URL.
						 Or it is a host name, which are also magically URLs.
						 */
					  URL_Struct *url = NET_CreateURLStruct (argv [i], NET_DONT_RELOAD);
				  
#ifdef MOZ_MAIL_NEWS
					  if (!XP_STRNCASECMP (argv[i], "mailto:", 7))
						type = MWContextMessageComposition;
					  else if (!XP_STRNCASECMP (argv[i], "addrbk:", 7))
						type = MWContextAddressBook;
#endif
				  
#ifdef NON_BETA
					  plonkp = True;
#endif
					  fe_MakeWindow (toplevel, 0, url, NULL, type, FALSE);
					}
				  else
					{
					  /* There is a / or end-of-string before
						 so it's a file. */
					  char cwd [1024];
					  URL_Struct *url;
#ifdef SUNOS4
					  if (! getwd (cwd))
#else
						if (! getcwd (cwd, sizeof cwd))
#endif
						  {
							fprintf (stderr, "%s: getwd: %s\n", 
									 fe_progname, cwd);
							break;
						  }
					  PR_snprintf (buf, sizeof (buf),
								   "file:%.900s/%.900s", cwd, argv [i]);
					  url = NET_CreateURLStruct (buf, NET_DONT_RELOAD);
#ifdef NON_BETA
					  plonkp = True;
#endif
					  fe_MakeWindow (toplevel, 0, url, NULL, MWContextBrowser,
									 FALSE);
					}
#ifdef NETHELP_STARTUP_FLAG
				  if (fe_globalData.startup_nethelp)
					{
					  XP_NetHelp(NULL, HELP_COMMUNICATOR);
					  type = MWContextBrowser;
					}
#endif
		  }
	  } /* end for */

#ifdef MOZ_MAIL_NEWS
	  /* -mail: show inbox */
	  if (fe_globalData.startup_mail)
	  {
		  fe_showInbox(toplevel, NULL, NULL,
					   fe_globalPrefs.reuse_thread_window, False);
	  }
	  
	  /* -news: show news */
	  if (fe_globalData.startup_news)
	  {
		  fe_showNewsgroups(toplevel, NULL, NULL);
	  }
#endif  /* MOZ_MAIL_NEWS */
	  
	  /* -bookmarks: show bookmarks */
	  if (fe_globalData.startup_bookmarks)
	  {
		  fe_showBookmarks(toplevel,NULL,NULL);
	  }
	  
	  /* -history: show history */
	  if (fe_globalData.startup_history)
	  {
		  fe_showHistory(toplevel,NULL,NULL);
	  }
  }
  else
  {
	  /* These command line options override the pres */
	  if (
#ifdef MOZ_TASKBAR
	          fe_globalData.startup_component_bar || 
#endif
#ifdef MOZ_MAIL_NEWS
		  fe_globalData.startup_composer ||
		  fe_globalData.startup_mail ||
		  fe_globalData.startup_news ||
#endif
		  fe_globalData.startup_nethelp)
	  {
#ifdef MOZ_TASKBAR
		  /* -component-bar: show the task bar only */
		  if (fe_globalData.startup_component_bar)
		  {
			  extern void fe_showTaskBar(Widget toplevel);
			  extern fe_colormap * fe_GetSharedColormap(void);

			  fe_colormap *cmap = fe_GetSharedColormap();
			  
			  MWContext * task_bar_context;

			  /* Make sure the bookmark frame is alive, since the floating
				 task bar uses it as its parent frame */
			  fe_createBookmarks(toplevel, NULL, NULL);
			  
			  task_bar_context = XP_NewContext();
			  
			  task_bar_context->fe.data = XP_NEW_ZAP(fe_ContextData);
			  
			  CONTEXT_DATA(task_bar_context)->colormap = cmap;
			  
			  fe_InitIconColors(task_bar_context);
			  
			  fe_showTaskBar(toplevel);

			  XP_FREE(CONTEXT_DATA(task_bar_context));
			  XP_FREE(task_bar_context);
		  }
		  else
#endif
		  {
#ifdef MOZ_MAIL_NEWS
			  /* -composer: show the composer */
			  if (fe_globalData.startup_composer)
			  {
				  fe_MakeWindow(toplevel, 0, NULL, NULL,MWContextEditor,FALSE);
			  }

			  /* -mail: show inbox */
			  if (fe_globalData.startup_mail)
			  {
				  fe_showInbox(toplevel, NULL, NULL,
							   fe_globalPrefs.reuse_thread_window, False);
			  }

			  /* -news: show news */
			  if (fe_globalData.startup_news)
			  {
				  fe_showNewsgroups(toplevel, NULL, NULL);
			  }
#endif  /* MOZ_MAIL_NEWS */

			  /* -nethelp: show nethelp only (no args)*/
			  if (fe_globalData.startup_nethelp)
			  {
#ifdef NETHELP_STARTUP_FLAG
				  /* we need some initial context, so create bookmarks */
				  fe_createBookmarks(toplevel, NULL, NULL);
				  /* the bookmarks context gets used in FE_GetNetHelpContext */
				  XP_NetHelp(NULL, HELP_COMMUNICATOR);

				  fe_globalData.startup_iconic = TRUE;
				  fe_MakeWindow (toplevel, 0, 0, NULL, MWContextBrowser, FALSE);
#endif
			  }

			  /*
			   * These two only get shown if one of the above was shown.
			   * They can only exist if another frame is shown.
			   */

			  /* -bookmarks: show bookmarks */
			  if (fe_globalData.startup_bookmarks)
			  {
				  fe_showBookmarks(toplevel,NULL,NULL);
			  }
	  
			  /* -history: show history */
			  if (fe_globalData.startup_history)
			  {
				  fe_showHistory(toplevel,NULL,NULL);
			  }
		  }
	  }
	  else
	  {
		  URL_Struct *url = 0;
		  XP_Bool     launch_appl = FALSE;

		  /*
		   * startup_mode in Navigator 4.0 is obsolete as we allow the user
		   * to launch more than one application during startup. 
		   *
		   * if (fe_globalPrefs.startup_mode == 1)
		   *     bufp = XP_STRDUP ("mailbox:");
		   * else if (fe_globalPrefs.startup_mode == 2)
		   *     bufp = XP_STRDUP ("news:");
		   * else {
		   * if (fe_globalPrefs.home_document && *fe_globalPrefs.home_document)
		   *         bufp = XP_STRDUP (fe_globalPrefs.home_document);
		   * }
		   *
		   * url = NET_CreateURLStruct (bufp, FALSE);
		   * fe_MakeWindow (toplevel, 0, url, NULL, MWContextBrowser, FALSE);
		   * if (bufp) XP_FREE (bufp);
		   */

		  /* Make sure we pop up the browser if no application is specified */
		  launch_appl = fe_globalPrefs.startup_browser_p ||
#ifdef MOZ_MAIL_NEWS
			  fe_globalPrefs.startup_mail_p ||
			  fe_globalPrefs.startup_news_p ||
#endif
#ifdef EDITOR
			  fe_globalPrefs.startup_editor_p ||
#endif
			  fe_globalPrefs.startup_conference_p ||
			  fe_globalPrefs.startup_netcaster_p ||
			  fe_globalData.startup_netcaster ||
			  fe_globalPrefs.startup_calendar_p;
		  
		  if ((fe_globalPrefs.startup_browser_p == True) ||
			  (! launch_appl)) {
			  url = fe_GetBrowserStartupUrlStruct();
			  fe_MakeWindow(toplevel, 0, url, NULL, MWContextBrowser, FALSE);
		  }
		  
#ifdef MOZ_MAIL_NEWS
		  if (fe_globalPrefs.startup_mail_p == True) {
			  fe_showInbox(toplevel, NULL, NULL, fe_globalPrefs.reuse_thread_window, False);
		  }
		  
		  if (fe_globalPrefs.startup_news_p == True) 
		  {
			  fe_showNewsgroups(toplevel, NULL, NULL);
		  }
#endif
#ifdef EDITOR
		  if (fe_globalPrefs.startup_editor_p == True) {
			  fe_showEditor(toplevel, NULL, NULL, NULL);
		  }
#endif
#ifdef MOZ_MAIL_NEWS

		  if (fe_globalPrefs.startup_conference_p == True) {
			  if (fe_IsConferenceInstalled())
				  fe_showConference(toplevel, 0 /* email */,
									0 /* use */, 0 /* coolAddr */);
		  }
#endif  /* MOZ_MAIL_NEWS */
		  
		  if (fe_globalPrefs.startup_netcaster_p == True) {
		    fe_globalData.startup_netcaster=False;
		    fe_showNetcaster(toplevel);
		  }
		  
		  if (fe_globalPrefs.startup_calendar_p == True) {
		    if (fe_IsCalendarInstalled())
		      fe_showCalendar(toplevel);
		  }
		}
	}

  if (fe_globalData.startup_netcaster)
    fe_showNetcaster(toplevel);

#ifdef MOZ_MAIL_NEWS
  /* Create the biffcontext, to keep the new-mail notifiers up-to-date. */
  biffcontext = XP_NewContext();
  biffcontext->type = MWContextBiff;
  biffcontext->funcs = fe_BuildDisplayFunctionTable();
  CONTEXT_DATA(biffcontext) = XP_NEW_ZAP(fe_ContextData);
  XP_AddContextToList(biffcontext);
  MSG_BiffInit(biffcontext, fe_mailNewsPrefs);
#endif

  fe_command_line_done = True;

  {
    extern void PR_SetXtHackOkayToReleaseXLockFn(int (*fn)(void));
    PR_SetXtHackOkayToReleaseXLockFn(fe_xt_hack_okayToReleaseXLock);
  }

  while (1)
    fe_EventLoop ();

  return (0);
}

#ifdef _HPUX_SOURCE
/* Words cannot express how much HPUX!
   Sometimes rename("/u/jwz/.MCOM-cache", "/u/jwz/.netscape-cache")
   will fail and randomly corrupt memory (but only in the case where
   the first is a directory and the second doesn't exist.)  To avoid
   this, we fork-and-exec `mv' instead of using rename().
 */
# include <sys/wait.h>

# undef rename
# define rename hpux_www_
static int
rename (const char *source, const char *target)
{
  struct sigaction newact, oldact;
  pid_t forked;
  int ac = 0;
  char **av = (char **) malloc (10 * sizeof (char *));
  av [ac++] = strdup ("/bin/mv");
  av [ac++] = strdup ("-f");
  av [ac++] = strdup (source);
  av [ac++] = strdup (target);
  av [ac++] = 0;
	
  /* Setup signals so that SIGCHLD is ignored as we want to do waitpid */
  newact.sa_handler = SIG_DFL;
  newact.sa_flags = 0;
  sigfillset(&newact.sa_mask);
  sigaction (SIGCHLD, &newact, &oldact);

  switch (forked = fork ())
    {
    case -1:
      while (--ac >= 0)
	free (av [ac]);
      free (av);
      /* Reset SIGCHLD signal hander before returning */
      sigaction(SIGCHLD, &oldact, NULL);
      return -1;	/* fork() failed (errno is meaningful.) */
      break;
    case 0:
      {
	execvp (av[0], av);
	exit (-1);	/* execvp() failed (this exits the child fork.) */
	break;
      }
    default:
      {
	/* This is the "old" process (subproc pid is in `forked'.) */
	int status = 0;

	/* wait for `mv' to terminate. */
	pid_t waited_pid = waitpid (forked, &status, 0);

	/* Reset SIGCHLD signal hander before returning */
	sigaction(SIGCHLD, &oldact, NULL);

	while (--ac >= 0)
	  free (av [ac]);
	free (av);

	return 0;
	break;
      }
    }
}
#endif /* !HPUX */



/* With every release, we change the names or locations of the init files.
   Is this a feature?

              1.0                       1.1                      2.0
  ========================== ======================== ========================
  .MCOM-bookmarks.html       .netscape-bookmarks.html .netscape/bookmarks.html
  .MCOM-HTTP-cookie-file     .netscape-cookies        .netscape/cookies
  .MCOM-preferences          .netscape-preferences    .netscape/preferences
  .MCOM-global-history       .netscape-history        .netscape/history
  .MCOM-cache/               .netscape-cache/         .netscape/cache/
  .MCOM-cache/MCOM-cache-fat .netscape-cache/index    .netscape/cache/index.db
  .MCOM-newsgroups-*         .netscape-newsgroups-*   .netscape/newsgroups-*
   <none>                     <none>                  .netscape/cert.db
   <none>                     <none>                  .netscape/cert-nameidx.db
   <none>                     <none>                  .netscape/key.db
   <none>                     <none>                  .netscape/popstate
   <none>                     <none>                  .netscape/cachelist
   <none>                     <none>                  nsmail/
   <none>                     <none>                  .netscape/mailsort

  Files that never changed:

  .newsrc-*                  .newsrc-*                .newsrc-*
  .snewsrc-*                 .snewsrc-*               .snewsrc-*
 */

#ifdef OLD_UNIX_FILES


static void
fe_rename_init_files (Widget toplevel)
{
  struct stat st;
  char file1 [1024];
  char file2 [1024];
  char buf [1024];
  char *s1, *s2;
  Boolean asked = False;

  PR_snprintf (file1, sizeof (file1), "%s/", fe_home_dir);
  strcpy (file2, file1);
  s1 = file1 + strlen (file1);
  s2 = file2 + strlen (file2);

#define FROB(NAME1,NAME2,GAG)						\
  strcpy (s1, NAME1);							\
  strcpy (s2, NAME2);							\
  if (stat (file2, &st))						\
    if (!stat (file1, &st))						\
      {									\
	if (! asked)							\
	  {								\
	    if (fe_Confirm_2 (toplevel,					\
			      fe_globalData.rename_files_message))	\
	      asked = True;						\
	    else							\
	      return;							\
	  }								\
	if (rename (file1, file2))					\
	  {								\
	    PR_snprintf (buf, sizeof (buf),
					 XP_GetString(XFE_MOZILLA_RENAMING_SOMETHING_TO_SOMETHING),
					 s1, s2); \
	    fe_perror_2 (toplevel, buf);				\
	  }								\
        else if (GAG)							\
	  fe_renamed_cache_dir = strdup (file2);			\
      }

  FROB (".MCOM-bookmarks.html",		  ".netscape-bookmarks.html", False);
  FROB (".MCOM-HTTP-cookie-file",	  ".netscape-cookies", False);
  FROB (".MCOM-preferences",		  ".netscape-preferences", False);
  FROB (".MCOM-global-history",		  ".netscape-history", False);
  FROB (".MCOM-cache",			  ".netscape-cache", True);
  FROB (".netscape-cache/MCOM-cache-fat", ".netscape-cache/index", False);
#undef FROB
}

#else  /* !OLD_UNIX_FILES */

#if !defined(__FreeBSD__) && !defined(MKLINUX) && !defined(LINUX_GLIBC_2)
extern char *sys_errlist[];
extern int sys_nerr;
#endif

static XP_Bool
fe_ensure_config_dir_exists (Widget toplevel)
{
  char *dir, *fmt;
  static char buf [2048];
  struct stat st;
  XP_Bool exists;

  dir = PR_smprintf ("%s/.netscape", fe_home_dir);
  if (!dir)
    return FALSE;

  exists = !stat (dir, &st);

  if (exists && !(st.st_mode & S_IFDIR))
    {
      /* It exists but is not a directory!
	 Rename the old file so that we can create the directory.
	 */
      char *loser = (char *) XP_ALLOC (XP_STRLEN (dir) + 100);
      XP_STRCPY (loser, dir);
      XP_STRCAT (loser, ".BAK");
      if (rename (dir, loser) == 0)
	{
	  fmt = XP_GetString( XFE_EXISTED_BUT_WAS_NOT_A_DIRECTORY );
	  exists = FALSE;
	}
      else
	{
	  fmt = XP_GetString( XFE_EXISTS_BUT_UNABLE_TO_RENAME );
	}

      PR_snprintf (buf, sizeof (buf), fmt, XP_AppName, dir, loser);
      XP_FREE (loser);
      fe_Alert_2 (toplevel, buf);
      if (exists)
	{
	  free (dir);
	  return FALSE;
	}
    }

  if (!exists)
    {
      /* ~/.netscape/ does not exist.  Create the directory.
       */
      if (mkdir (dir, 0700) < 0)
	{
	  fmt = XP_GetString( XFE_UNABLE_TO_CREATE_DIRECTORY );
	  PR_snprintf (buf, sizeof (buf), fmt, XP_AppName, dir,
		   ((errno >= 0 && errno < sys_nerr)
		    ? sys_errlist [errno] : XP_GetString( XFE_UNKNOWN_ERROR )));
	  fe_Alert_2 (toplevel, buf);
	  free (dir);
	  return FALSE;
	}
    }

  fe_config_dir = dir;
  return TRUE;
}

#ifndef SUNOS4
void
fe_atexit_handler(void)
{
  fe_MinimalNoUICleanup();
  if (fe_pidlock) unlink (fe_pidlock);
  fe_pidlock = NULL;
}
#endif /* SUNOS4 */

#ifndef INADDR_LOOPBACK
#define	INADDR_LOOPBACK		0x7F000001
#endif

#define MAXTRIES	100

static int
fe_create_pidlock (const char *name, unsigned long *paddr, pid_t *ppid)
{
  char hostname[64];		/* should use MAXHOSTNAMELEN */
  PRHostEnt *hp, hpbuf;
  char dbbuf[PR_NETDB_BUF_SIZE];
  unsigned long myaddr, addr;
  struct in_addr inaddr;
  char *signature;
  int len, tries, rval;
  char buf[1024];		/* should use PATH_MAX or pathconf */
  char *colon, *after;
  pid_t pid;

#ifndef NSPR20
  if (gethostname (hostname, sizeof hostname) < 0 ||
      (hp = PR_gethostbyname (hostname, &hpbuf, dbbuf, sizeof(dbbuf), 0)) == NULL)
    inaddr.s_addr = INADDR_LOOPBACK;
  else
    memcpy (&inaddr, hp->h_addr, sizeof inaddr);
#else
  {
  PRStatus sts;

  sts = PR_GetSystemInfo(PR_SI_HOSTNAME, hostname, sizeof(hostname));
  if (sts == PR_FAILURE)
    inaddr.s_addr = INADDR_LOOPBACK;
  else {
    sts = PR_GetHostByName (hostname, dbbuf, sizeof(dbbuf), &hpbuf);
    if (sts == PR_FAILURE)
      inaddr.s_addr = INADDR_LOOPBACK;
    else {
      hp = &hpbuf;
      memcpy (&inaddr, hp->h_addr, sizeof inaddr);
    }
  }
  }
#endif

  myaddr = inaddr.s_addr;
  signature = PR_smprintf ("%s:%u", inet_ntoa (inaddr), (unsigned)getpid ());
  tries = 0;
  addr = 0;
  pid = 0;
  while ((rval = symlink (signature, name)) < 0)
    {
      if (errno != EEXIST)
	break;
      len = readlink (name, buf, sizeof buf - 1);
      if (len > 0)
	{
	  buf[len] = '\0';
	  colon = strchr (buf, ':');
	  if (colon != NULL)
	    {
	      *colon++ = '\0';
	      if ((addr = inet_addr (buf)) != (unsigned long)-1)
		{
		  pid = strtol (colon, &after, 0);
		  if (pid != 0 && *after == '\0')
		    {
		      if (addr != myaddr)
			{
			  /* Remote pid: give up even if lock is stuck. */
			  break;
			}

		      /* signator was a local process -- check liveness */
		      if (kill (pid, 0) == 0 || errno != ESRCH)
			{
			  /* Lock-owning process appears to be alive. */
			  break;
			}
		    }
		}
	    }
	}

      /* Try to claim a bogus or stuck lock. */
      (void) unlink (name);
      if (++tries > MAXTRIES)
	break;
    }

  if (rval == 0)
    {
      struct sigaction act, oldact;

      act.sa_handler = fe_Exit;
      act.sa_flags = 0;
      sigfillset (&act.sa_mask);

      /* Set SIGINT, SIGTERM and SIGHUP to our fe_Exit(). If these signals
       * have already been ignored, dont install our handler because we
       * could have been started up in the background with a nohup.
       */
      sigaction (SIGINT, NULL, &oldact);
      if (oldact.sa_handler != SIG_IGN)
	sigaction (SIGINT, &act, NULL);

      sigaction (SIGHUP, NULL, &oldact);
      if (oldact.sa_handler != SIG_IGN)
	sigaction (SIGHUP, &act, NULL);

      sigaction (SIGTERM, NULL, &oldact);
      if (oldact.sa_handler != SIG_IGN)
	sigaction (SIGTERM, &act, NULL);

#ifndef SUNOS4
      /* atexit() is not available in sun4. We need to find someother
       * mechanism to do this in sun4. Maybe we will get a signal or
       * something.
       */

      /* Register a atexit() handler to remove lock file */
      atexit(fe_atexit_handler);
#endif /* SUNOS4 */
    }
  free (signature);
  *paddr = addr;
  *ppid = pid;
  return rval;
}


/* This copies one file to another, optionally setting the permissions.
 */
static void
fe_copy_file (Widget toplevel, const char *in, const char *out, int perms)
{
  char buf [1024];
  FILE *ifp, *ofp;
  int n;
  ifp = fopen (in, "r");
  if (!ifp) return;
  ofp = fopen (out, "w");
  if (!ofp)
    {
      PR_snprintf (buf, sizeof (buf), XP_GetString( XFE_ERROR_CREATING ), out);
      fe_perror_2 (toplevel, buf);
      return;
    }

  if (perms)
    fchmod (fileno (ofp), perms);

  while ((n = fread (buf, 1, sizeof(buf), ifp)) > 0)
    while (n > 0)
      {
	int w = fwrite (buf, 1, n, ofp);
	if (w < 0)
	  {
	    PR_snprintf (buf, sizeof (buf), XP_GetString( XFE_ERROR_WRITING ), out);
	    fe_perror_2 (toplevel, buf);
	    return;
	  }
	n -= w;
      }
  fclose (ofp);
  fclose (ifp);
}


/* This does several things that need to be done before the preferences
   file is loaded:

   - cause the directory ~/.netscape/ to exist,
     and warn if it couldn't be created

   - cause these files to be *copied* from their older
     equivalents, if there are older versions around

     ~/.netscape/preferences
     ~/.netscape/bookmarks.html
     ~/.netscape/cookies
 */
static XP_Bool fe_copied_init_files = FALSE;

static void
fe_copy_init_files (Widget toplevel)
{
  struct stat st1;
  struct stat st2;
  char file1 [512];
  char file2 [512];
  char *s1, *s2;

  if (!fe_config_dir)
    /* If we were unable to cause ~/.netscape/ to exist, give up now. */
    return;

  PR_snprintf (file1, sizeof (file1), "%s/", fe_home_dir);
  strcpy (file2, file1);
  s1 = file1 + strlen (file1);
  s2 = file2 + strlen (file2);

#define FROB(OLD1, OLD2, NEW, PERMS)			\
  strcpy (s1, OLD1);					\
  strcpy (s2, NEW);					\
  if (!stat (file2, &st2))				\
    ;    /* new exists - leave it alone */		\
  else if (!stat (file1, &st1))				\
    {							\
      fe_copied_init_files = TRUE;			\
      fe_copy_file (toplevel, file1, file2, 0);		\
    }							\
  else							\
    {							\
      strcpy (s1, OLD2);				\
      if (!stat (file1, &st1))				\
	{						\
          fe_copied_init_files = TRUE;			\
	  fe_copy_file (toplevel, file1, file2, 0);	\
        }						\
    }

  FROB(".netscape-preferences",
       ".MCOM-preferences",
       ".netscape/preferences",
       0)
  FROB(".netscape-bookmarks.html",
       ".MCOM-bookmarks.html",
       ".netscape/bookmarks.html",
       0)

  FROB(".netscape-cookies",
       ".MCOM-HTTP-cookie-file",
       ".netscape/cookies",
       (S_IRUSR | S_IWUSR))		/* rw only by owner */

#undef FROB
}


/* This does several things that need to be done after the preferences
   file is loaded:

   - if files were renamed, change the historical values in the preferences
     file to the new values, and save it out again

   - offer to delete any obsolete ~/.MCOM or ~/.netscape- that are still
     sitting around taking up space
 */
static void
fe_clean_old_init_files (Widget toplevel)
{
  char buf[1024];

  char nbm[512],    mbm[512];
  char ncook[512],  mcook[512];
  char npref[512],  mpref[512];
  char nhist[512],  mhist[512];
  char ncache[512], mcache[512];


  XP_Bool old_stuff = FALSE;
  XP_Bool old_cache = FALSE;

  char *slash;
  struct stat st;

  /* spider begin */
  /* TODO: where does this string get free'd? */
  if (fe_globalPrefs.sar_cache_dir) free (fe_globalPrefs.sar_cache_dir);
  PR_snprintf (buf, sizeof (buf), "%s/.netscape/archive/", fe_home_dir);
  fe_globalPrefs.sar_cache_dir = strdup (buf);
  /* spider end */

  if (!fe_copied_init_files)
    return;

  /* History and cache always go in the new place by default,
     no matter what they were set to before. */
  if (fe_globalPrefs.history_file) free (fe_globalPrefs.history_file);
  PR_snprintf (buf, sizeof (buf), "%s/.netscape/history.db", fe_home_dir);
  fe_globalPrefs.history_file = strdup (buf);

  if (fe_globalPrefs.cache_dir) free (fe_globalPrefs.cache_dir);
  PR_snprintf (buf, sizeof (buf), "%s/.netscape/cache/", fe_home_dir);
  fe_globalPrefs.cache_dir = strdup (buf);

  /* If they were already keeping their bookmarks file in a different
     place, don't change that preferences setting. */
  PR_snprintf (buf, sizeof (buf), "%s/.netscape-bookmarks.html", fe_home_dir);
  if (!fe_globalPrefs.bookmark_file ||
      !XP_STRCMP (fe_globalPrefs.bookmark_file, buf))
    {
      if (fe_globalPrefs.bookmark_file) free (fe_globalPrefs.bookmark_file);
      PR_snprintf (buf, sizeof (buf), "%s/.netscape/bookmarks.html",
		   fe_home_dir);
      fe_globalPrefs.bookmark_file = strdup (buf);
    }

  /* If their home page was set to their bookmarks file (and that was
     the default location) then move it to the new place too. */
  PR_snprintf (buf, sizeof (buf), "file:%s/.netscape-bookmarks.html",
	       fe_home_dir);
  if (!fe_globalPrefs.home_document ||
      !XP_STRCMP (fe_globalPrefs.home_document, buf))
    {
      if (fe_globalPrefs.home_document) free (fe_globalPrefs.home_document);
      PR_snprintf (buf, sizeof (buf), "file:%s/.netscape/bookmarks.html",
		   fe_home_dir);
      fe_globalPrefs.home_document = strdup (buf);
    }

  fe_copied_init_files = FALSE;

  if (!XFE_SavePrefs ((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
    fe_perror_2 (toplevel,XP_GetString(XFE_MOZILLA_ERROR_SAVING_OPTIONS));


  PR_snprintf (nbm, sizeof (nbm), "%s/", fe_home_dir);

  strcpy (mbm, nbm);    strcat (mbm,   ".MCOM-bookmarks.html");
  strcpy (ncook, nbm);  strcat (ncook, ".netscape-cookies");
  strcpy (mcook, nbm);  strcat (mcook, ".MCOM-HTTP-cookie-file");
  strcpy (npref, nbm);  strcat (npref, ".netscape-preferences");
  strcpy (mpref, nbm);  strcat (mpref, ".MCOM-preferences");
  strcpy (nhist, nbm);  strcat (nhist, ".netscape-history");
  strcpy (mhist, nbm);  strcat (mhist, ".MCOM-global-history");
  strcpy (ncache, nbm); strcat (ncache,".netscape-cache");
  strcpy (mcache, nbm); strcat (mcache,".MCOM-cache");
                        strcat (nbm,   ".netscape-bookmarks.html");

  if (stat (nbm, &st) == 0   || stat (mbm, &st) == 0 ||
      stat (ncook, &st) == 0 || stat (mcook, &st) == 0 ||
      stat (npref, &st) == 0 || stat (mpref, &st) == 0 ||
      stat (nhist, &st) == 0 || stat (mhist, &st) == 0)
    old_stuff = TRUE;

  if (stat (ncache, &st) == 0 || stat (mcache, &st) == 0)
    old_stuff = old_cache = TRUE;

  if (old_stuff)
    {
      Boolean doit;
      char *fmt = XP_GetString( XFE_CREATE_CONFIG_FILES );

      char *foo =
	(old_cache
	 ? XP_GetString( XFE_OLD_FILES_AND_CACHE )
	 : XP_GetString( XFE_OLD_FILES ) );

      PR_snprintf (buf, sizeof (buf), fmt, XP_AppName, foo);

      doit = (Bool) ((int) fe_dialog (toplevel, "convertQuestion", buf,
				  TRUE, 0, TRUE, FALSE, 0));
      if (doit)
	{
	  DIR *dir;

	  /* Nuke the simple files */
	  unlink (nbm);   unlink (mbm);
	  unlink (ncook); unlink (mcook);
	  unlink (npref); unlink (mpref);
	  unlink (nhist); unlink (mhist);

	  /* Nuke the 1.1 cache directory and files in it */
	  dir = opendir (ncache);
	  if (dir)
	    {
	      struct dirent *dp;
	      strcpy (buf, ncache);
	      slash = buf + strlen (buf);
	      *slash++ = '/';
	      while ((dp = readdir (dir)))
		{
		  strcpy (slash, dp->d_name);
		  unlink (buf);
		}
	      closedir (dir);
	      rmdir (ncache);
	    }

	  /* Nuke the 1.0 cache directory and files in it */
	  dir = opendir (mcache);
	  if (dir)
	    {
	      struct dirent *dp;
	      strcpy (buf, mcache);
	      slash = buf + strlen (buf);
	      *slash++ = '/';
	      while ((dp = readdir (dir)))
		{
		  if (strncmp (dp->d_name, "cache", 5) &&
		      strcmp (dp->d_name, "index") &&
		      strcmp (dp->d_name, "MCOM-cache-fat"))
		    continue;
		  strcpy (slash, dp->d_name);
		  unlink (buf);
		}
	      closedir (dir);
	      rmdir (mcache);
	    }

	  /* Now look for the saved-newsgroup-listings in ~/. and nuke those.
	     (We could rename them, but I just don't care.)
	   */
	  slash = strrchr (nbm, '/');
	  slash[1] = '.';
	  slash[2] = 0;
	  dir = opendir (nbm);
	  if (dir)
	    {
	      struct dirent *dp;
	      while ((dp = readdir (dir)))
		{
		  if (dp->d_name[0] != '.')
		    continue;
		  if (!strncmp (dp->d_name, ".MCOM-newsgroups-", 17) ||
		      !strncmp (dp->d_name, ".netscape-newsgroups-", 21))
		    {
		      strcpy (slash+1, dp->d_name);
		      unlink (nbm);
		    }
		}
	      closedir (dir);
	    }
	}
    }
}

#endif /* !OLD_UNIX_FILES */

#ifdef MOZ_MAIL_NEWS

/*
 * Message Composition
 */

extern void
fe_mc_field_changed(Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext* context = fe_WidgetToMWContext(widget);
  MSG_HEADER_SET msgtype = (MSG_HEADER_SET) closure;
  char* value = 0;
  XP_ASSERT (context);
  if (!context) return;
  value = fe_GetTextField(widget);
  MSG_SetCompHeader (CONTEXT_DATA(context)->comppane, msgtype, value);
}


extern void
fe_mc_field_lostfocus(Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext* context = fe_WidgetToMWContext(widget);
  MSG_HEADER_SET msgtype = (MSG_HEADER_SET) closure;
  char* value = 0;
  char* newvalue;
  XP_ASSERT (context);
  if (!context) return;
  value = fe_GetTextField(widget);
  newvalue = MSG_UpdateHeaderContents(CONTEXT_DATA(context)->comppane,
				      msgtype, value);
  if (newvalue) {
	fe_SetTextField(widget, newvalue);
    MSG_SetCompHeader(CONTEXT_DATA(context)->comppane, msgtype, newvalue);
    XP_FREE(newvalue);
  }
}

extern void
fe_browse_file_of_text (MWContext *context, Widget text_field, Boolean dirp);

static void
fe_mailto_browse_cb(Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext* context = (MWContext*) closure;
  fe_browse_file_of_text(context, CONTEXT_DATA(context)->mcFcc, FALSE); 
}

#define Off(field) XtOffset(fe_ContextData*, field)
#define ISLABEL		0x0001
#define ADDBROWSE	0x0002
#define ISDROPSITE	0x0008
static struct {
  char* name;
  int offset;
  MSG_HEADER_SET msgtype;
  int flags;
} description[] = {
  {"from",	Off(mcFrom),		MSG_FROM_HEADER_MASK, ISLABEL},
  {"replyTo", 	Off(mcReplyTo),		MSG_REPLY_TO_HEADER_MASK, ISDROPSITE},
  {"to",	Off(mcTo),		MSG_TO_HEADER_MASK, ISDROPSITE},
  {"cc",	Off(mcCc),		MSG_CC_HEADER_MASK, ISDROPSITE},
  {"bcc",	Off(mcBcc),		MSG_BCC_HEADER_MASK, ISDROPSITE},
  {"fcc",	Off(mcFcc),		MSG_FCC_HEADER_MASK,ADDBROWSE},
  {"newsgroups",Off(mcNewsgroups),	MSG_NEWSGROUPS_HEADER_MASK, 0},
  {"followupTo",Off(mcFollowupTo),	MSG_FOLLOWUP_TO_HEADER_MASK,0},
  {"attachments",Off(mcAttachments),	MSG_ATTACHMENTS_HEADER_MASK, ISDROPSITE},
};
#define NUM (sizeof(description) / sizeof(description[0]))
/*
  {"subject", 	Off(mcSubject),		MSG_SUBJECT_HEADER_MASK, 0},
*/

#endif  /* MOZ_MAIL_NEWS */

void resize(Widget w, XtPointer clientData, XtPointer callData)
{
    Widget form = (Widget)clientData;
 
    XtVaSetValues(form, XmNwidth, XfeWidth(w), 0 );
    XmUpdateDisplay(XtParent(XtParent(form)));

 
}

void expose(Widget w, XtPointer clientData, XtPointer callData)
{
    Widget form = (Widget)clientData;
 
    XtVaSetValues(form, XmNwidth, XfeWidth(w), 0 );
 
}

#ifdef MOZ_MAIL_NEWS

WidgetList
fe_create_composition_widgets(MWContext* context, Widget pane, int *numkids)
{

  Widget* form;
  Widget label[NUM];
  Widget widget[NUM];
  Widget other[NUM];
  Widget kids[10]; 
  /*XmFontList fontList;*/
  Arg av [20];
  int ac = 0;
  int i;
  char buf[100];
  int maxwidth = 0;

  
  fe_ContextData* data = CONTEXT_DATA(context);
  Widget formParent;
  Widget scroller;
  Widget drawingArea;
  Widget hsb, vsb;
 
  XtVaSetValues(pane, XmNseparatorOn, True, 0);
  ac = 0;
  XtSetArg(av[ac], XmNscrollingPolicy, XmAUTOMATIC); ac++;
  scroller = XmCreateScrolledWindow(pane,  "scrollerX", av, ac);
  XtManageChild(scroller);
 
  XtVaGetValues(scroller, XmNclipWindow, &drawingArea,
                XmNhorizontalScrollBar, &hsb,
                XmNverticalScrollBar, &vsb, 0 );
 
  XtVaSetValues(hsb, 
		XmNhighlightThickness, 0,
		 0);
  XtVaSetValues(vsb, 
		XmNhighlightThickness, 0,
		 0);
 
 
  ac = 0;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  formParent = XmCreateForm(scroller, "mailto_parent", av, ac);
  XtManageChild(formParent);
  XtAddCallback(drawingArea, XmNresizeCallback, resize, formParent);
  XtAddCallback(drawingArea, XmNexposeCallback, expose, formParent);
 
  *numkids = 0;
  form = (WidgetList)XtMalloc(sizeof(Widget)*(NUM+1));
 
  XtVaSetValues(pane, XmNseparatorOn, False, 0);

  for (i=0 ; i<NUM ; i++) {
    int flags = description[i].flags;
    ac = 0;
    form[i] = XmCreateForm(formParent, "mailto_field", av, ac);
    other[i] = NULL;
    if (flags & (ADDBROWSE)) {
      ac = 0;

      XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
      XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
      XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
      other[i] = XmCreatePushButtonGadget(form[i], "browseButton", av, ac);
      XtAddCallback(other[i], XmNactivateCallback, fe_mailto_browse_cb,
		    context);
    }
    ac = 0;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    if (other[i]) {
      XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
      XtSetArg(av[ac], XmNrightWidget, other[i]); ac++;
    } else {
      XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    }
    if (flags & ISLABEL) {
      XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
      widget[i] = XmCreateLabelGadget(form[i], description[i].name, av, ac);
    } else {
      XtSetArg(av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
      XtSetArg(av[ac], XmNwordWrap, True); ac++;
      XtSetArg(av[ac], XmNcolumns, 30); ac++;
      XtSetArg(av[ac], XmNrows, 3); ac++;
      XtSetArg(av[ac], XmNresizeHeight, True); ac++;
      XtSetArg(av[ac], XmNresizeWidth, True); ac++;
      widget[i] = fe_CreateText(form[i], description[i].name, av, ac);
      XtAddCallback(widget[i], XmNvalueChangedCallback, fe_mc_field_changed,
		    (XtPointer) description[i].msgtype);
      XtAddCallback(widget[i], XmNlosingFocusCallback, fe_mc_field_lostfocus,
		    (XtPointer) description[i].msgtype);
    }
 
    *((Widget*) ((char*) data + description[i].offset)) = widget[i];
 
    ac = 0;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
    XtSetArg(av[ac], XmNrightWidget, widget[i]); ac++;
    PR_snprintf(buf, sizeof (buf), "%sLabel", description[i].name);
    label[i] = XmCreateLabelGadget(form[i], buf, av, ac);
    if (maxwidth < XfeWidth(label[i])) {
      maxwidth = XfeWidth(label[i]);
    }
 
 
  }
  for (i=0 ; i<NUM ; i++) {
    XtVaSetValues(widget[i], XmNleftOffset, maxwidth, 0);
    kids[0] = label[i];
    kids[1] = widget[i];
    kids[2] = other[i];
    XtManageChildren(kids, other[i] ? 3 : 2);
   }
    XtVaSetValues(form[0],
                XmNtopAttachment, XmATTACH_FORM,
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM, 0);
  for (i=1 ; i<NUM ; i++) {
    XtVaSetValues(form[i],
                XmNtopAttachment, XmATTACH_WIDGET,
                XmNtopWidget, form[i-1],
                XmNbottomAttachment, XmATTACH_NONE,
                XmNleftAttachment, XmATTACH_FORM,
                XmNrightAttachment, XmATTACH_FORM, 0);
  }
/*
    XtVaSetValues(form[i],
                  XmNpaneMinimum, XfeHeight(widget[i]),
                  XmNpaneMaximum, XfeHeight(widget[i]),
                  0);
*/

  *numkids = NUM;

  return form;
  /* XtManageChild(form[NUM]); Unfortunately, this makes the window be too
     small. */

}


void
FE_MsgShowHeaders (MSG_Pane* comppane, MSG_HEADER_SET headers)
{
  MWContext* context = MSG_GetContext(comppane);
  fe_ContextData* data = CONTEXT_DATA(context);
  int i;
  int oncount = 0;
  int offcount = 0;
  Widget on[20];
  Widget off[20];
  XP_ASSERT(context->type == MWContextMessageComposition);
  for (i=0 ; i<NUM ; i++) {
    Widget widget = *((Widget*) ((char*) data + description[i].offset));
    Widget parent = XtParent(widget);
    if (headers & description[i].msgtype) {
      on[oncount++] = parent;
    } else {
      off[offcount++] = parent;
    }
  }
  if (offcount) XtUnmanageChildren(off, offcount);
  if (oncount) XtManageChildren(on, oncount);
}

#undef NUM
#undef ADDBROWSE
#undef ISLABEL
#undef Off

#endif  /* MOZ_MAIL_NEWS */

#ifdef JAVA

#include <Xm/Label.h>

/*
** MOTIF. What else can you say?
*/
Widget
FE_MakeAppletSecurityChrome(Widget parent, char* warning)
{
    Widget warningWindow, label, form, sep, skinnyDrawingArea;
    XmString cwarning;
    Arg av[20];
    Widget sec_logo = 0;
    int ac = 0;
    MWContext* someRandomContext = NULL;
    Colormap cmap;
    Pixel fg;
    Pixel bg;

    someRandomContext = XP_FindContextOfType(NULL, MWContextBrowser);
    if (!someRandomContext)
	someRandomContext = XP_FindContextOfType(NULL, MWContextMail);
    if (!someRandomContext)
	someRandomContext = XP_FindContextOfType(NULL, MWContextNews);
    if (!someRandomContext)
	someRandomContext = fe_all_MWContexts->context;

    cmap = fe_cmap(someRandomContext);
    fg = CONTEXT_DATA(someRandomContext)->fg_pixel;
    bg = CONTEXT_DATA(someRandomContext)->default_bg_pixel;

    ac = 0;
    XtSetArg(av[ac], XmNspacing, 0); ac++;
    XtSetArg(av[ac], XmNheight, 20); ac++;
    XtSetArg(av[ac], XmNmarginHeight, 0); ac++;
    XtSetArg(av[ac], XmNmarginWidth, 0); ac++;
    XtSetArg(av[ac], XmNcolormap, cmap); ac++;
    XtSetArg(av[ac], XmNforeground, fg); ac++;
    XtSetArg(av[ac], XmNbackground, bg); ac++;
    XtSetArg(av[ac], XmNresizable, False); ac++;
    warningWindow = XmCreateForm(parent, "javaWarning", av, ac);

    /* We make this widget to give the seperator something to seperate. */
    ac = 0;
    XtSetArg(av[ac], XmNheight, 1); ac++;
    XtSetArg(av[ac], XmNcolormap, cmap); ac++;
    XtSetArg(av[ac], XmNbackground, bg); ac++;
    XtSetArg(av[ac], XmNresizePolicy, XmRESIZE_NONE); ac++;
    skinnyDrawingArea = XmCreateDrawingArea(warningWindow, "javaDA", av, ac);

    ac = 0;
    sep = XmCreateSeparatorGadget(warningWindow, "javaSep", av, ac);

    ac = 0;
    XtSetArg(av[ac], XmNspacing, 0); ac++;
    XtSetArg(av[ac], XmNmarginHeight, 0); ac++;
    XtSetArg(av[ac], XmNmarginWidth, 0); ac++;
    XtSetArg(av[ac], XmNcolormap, cmap); ac++;
    XtSetArg(av[ac], XmNforeground, fg); ac++;
    XtSetArg(av[ac], XmNbackground, bg); ac++;
    XtSetArg(av[ac], XmNresizable, False); ac++;
    form = XmCreateForm(warningWindow, "javaForm", av, ac);

    cwarning = XmStringCreate (warning, XmFONTLIST_DEFAULT_TAG);
    ac = 0;
    XtSetArg(av[ac], XmNhighlightThickness, 0); ac++;
    XtSetArg(av[ac], XmNalignment, XmALIGNMENT_BEGINNING); ac++;
    XtSetArg(av[ac], XmNrecomputeSize, False); ac++;
    XtSetArg(av[ac], XmNcolormap, cmap); ac++;
    XtSetArg(av[ac], XmNforeground, fg); ac++;
    XtSetArg(av[ac], XmNbackground, bg); ac++;
    XtSetArg(av[ac], XmNlabelString, cwarning); ac++;
    label = XmCreateLabelGadget (form, "mouseDocumentation", av, ac);
    XmStringFree(cwarning);

#ifndef NO_SECURITY
    /*
    ** This section of displays security logos.  It won't compile
    ** properly if security is turned off.  Instead of ifdef'ing
    ** we might want to replace with a placeholder.
    */
    {
	Dimension w, h;
	Pixmap p = fe_SecurityPixmap(NULL, &w, &h, 0);
	ac = 0;
	XtSetArg(av[ac], XmNresizable, False); ac++;
	XtSetArg(av[ac], XmNlabelType, XmPIXMAP); ac++;
	XtSetArg(av[ac], XmNalignment, XmALIGNMENT_CENTER); ac++;
	XtSetArg(av[ac], XmNlabelPixmap, p); ac++;
	XtSetArg(av[ac], XmNwidth, w); ac++;
	XtSetArg(av[ac], XmNheight, h); ac++;
	XtSetArg(av[ac], XmNcolormap, cmap); ac++;
	XtSetArg (av [ac], XmNshadowThickness, 2); ac++;
	sec_logo = XmCreatePushButtonGadget (form, "javaSecLogo", av, ac);
    }
#endif /* ! NO_SECURITY */

    /* Now that widgets have been created, do the form attachments */
    XtVaSetValues(skinnyDrawingArea,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_NONE,
		  XmNleftAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_FORM,
		  0);

    XtVaSetValues(sep,
		  XmNtopAttachment, XmATTACH_WIDGET,
		  XmNtopWidget, skinnyDrawingArea,
		  XmNbottomAttachment, XmATTACH_NONE,
		  XmNleftAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_FORM,
		  0);

    XtVaSetValues(form,
		  XmNtopAttachment, XmATTACH_WIDGET,
		  XmNtopWidget, sep,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_FORM,
		  XmNrightAttachment, XmATTACH_FORM,
		  0);

    if (sec_logo) {
	XtVaSetValues(sec_logo,
		      XmNtopAttachment, XmATTACH_FORM,
		      XmNbottomAttachment, XmATTACH_FORM,
		      XmNleftAttachment, XmATTACH_FORM,
		      XmNleftOffset, 4,
		      XmNrightAttachment, XmATTACH_NONE,
		      0);
    }

    XtVaSetValues(label,
		  XmNtopAttachment, XmATTACH_FORM,
		  XmNbottomAttachment, XmATTACH_FORM,
		  XmNleftAttachment, XmATTACH_WIDGET,
		  XmNleftWidget, sec_logo,
		  XmNrightAttachment, XmATTACH_FORM,
		  0);

    XtManageChild(label);
    if (sec_logo)
	XtManageChild(sec_logo);
    XtManageChild(form);
    XtManageChild(sep);
    XtManageChild(skinnyDrawingArea);
    XtManageChild(warningWindow);
    return warningWindow;
}
#endif

/*
 * Sash Geometry is saved as
 * 	<pane-config>: w x h [;<pane-config>:wxh]
 *           %d       %dx%d
 *	or
 *	w x h
 *	%dx%d
 * for each type of pane configuration we store the widths and heights
 */

static char *
fe_ParseSashGeometry(char *old_geom_str, int pane_config,
			unsigned int *w, unsigned int *h, Boolean find_mode_p)
{
    char *geom_str = old_geom_str;
    char buf[100], *s, *t;
    int n;
    int tmppane, tmpw, tmph;

    char new_geom_str[512];
    int len = 0;

    new_geom_str[0] = '\0';

    while (geom_str && *geom_str) {

	for (s=buf, t=geom_str; *t && *t != ';'; ) *s++=*t++;
	*s = '\0';
	if (*t == ';') t++;
	geom_str = t;
	/* At end of this:
	 * geom_str - points to next section
	 * buf - has copy of this section
	 * t, s - tmp
	 */

	for (s = buf; *s && *s != ':'; s++);
	if (*s == ':') { 	/* New format */
	    n = sscanf(buf,"%d:%dx%d", &tmppane, &tmpw, &tmph);
	    if (n < 3) n = 0;
	}
	else {			/* Old format */
	    n = sscanf(buf,"%dx%d", &tmpw, &tmph);
	    if (n < 2) n = 0;
	    tmppane = FE_PANES_NORMAL;
	}
	/* At the end of this:
	 * n - '0' indicates badly formed section
	 * tmppane, tmpw, tmph - if n > 0 indicate %d:%dx%d
	 */

	if (find_mode_p && (n > 0) && tmppane == pane_config) {
	    *w = tmpw;
	    *h = tmph;
	    return(old_geom_str);
	}
	if (n != 0 && tmppane != pane_config) {
	    if (len != 0) {
		PR_snprintf(&new_geom_str[len], sizeof(new_geom_str)-len, ";");
		len++;
	    }
	    PR_snprintf(&new_geom_str[len], sizeof(new_geom_str)-len, 
				"%d:%dx%d", tmppane, tmpw, tmph);
	    len = strlen(new_geom_str);
	}
    }
    if (find_mode_p)
	return(NULL);
    else {
	if (*new_geom_str)
	    s = PR_smprintf("%d:%dx%d;%s", pane_config, *w, *h, new_geom_str);
	else
	    s = PR_smprintf("%d:%dx%d", pane_config, *w, *h);
	return(s);
    }
}

char *
fe_MakeSashGeometry(char *old_geom_str, int pane_config,
			unsigned int w, unsigned int h)
{
    char *new_geom_str =
	fe_ParseSashGeometry(old_geom_str, pane_config, &w, &h, False);
    return(new_geom_str);
}


void
fe_GetSashGeometry(char *geom_str, int pane_config,
			unsigned int *w, unsigned int *h)
{
    (void) fe_ParseSashGeometry(geom_str, pane_config, w, h, True);
    return;
}

extern XtPointer
fe_tooltip_mappee(Widget widget, XtPointer data)
{
    if (XtIsSubclass(widget, xmManagerWidgetClass)) 
	{
		if (XfeMenuIsOptionMenu(widget)
			||
			XtIsSubclass(widget, xmDrawingAreaWidgetClass)
			) 
		{
			fe_WidgetAddToolTips(widget);
		} 
		else if (fe_ManagerCheckGadgetToolTips(widget, NULL)) 
		{
			fe_ManagerAddGadgetToolTips(widget, NULL);
		}
    } 
	else if (XtIsSubclass(widget, xmPushButtonWidgetClass)
			 ||
			 XtIsSubclass(widget, xmToggleButtonWidgetClass)
			 ||
			 XtIsSubclass(widget, xmCascadeButtonWidgetClass)
			 ||
			 XtIsSubclass(widget, xfeButtonWidgetClass)
		) 
	{
		fe_WidgetAddToolTips(widget);
    }
	
    return NULL;
}

#ifdef LEDGES
static void
fe_configure_ledges (MWContext *context, int top_p, int bot_p)
{
  Widget top_ledge = CONTEXT_DATA (context)->top_ledge;
  Widget bot_ledge = CONTEXT_DATA (context)->bottom_ledge;
  Widget k1[2], k2[2];
  int c1 = 0, c2 = 0;

  if (top_p)
    k1[c1++] = top_ledge;
  else
    k2[c2++] = top_ledge;

  if (bot_p)
    k1[c1++] = bot_ledge;
  else
    k2[c2++] = bot_ledge;

  if (c1) XtManageChildren (k1, c1);
  if (c2) XtUnmanageChildren (k2, c2);
}
#endif /* LEDGES */


void
fe_sec_logo_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  SECNAV_SecurityAdvisor (context, SHIST_CreateURLStructFromHistoryEntry (context,
			       SHIST_GetCurrent (&context->hist)));
}

void
fe_AbortCallback (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XP_InterruptContext (context);

  /* If we were downloading, cleanup the file we downloaded to. */
  if (context->type == MWContextSaveToDisk) {
    char *filename = fe_GetTextField (CONTEXT_DATA(context)->url_label);
    XP_FileRemove(filename, xpTemporary);
  }
}


#include <X11/Xmu/Error.h>
#include <X11/Xproto.h>

static int
x_fatal_error_handler(Display *dpy)
{
  fe_MinimalNoUICleanup();
  if (fe_pidlock) unlink (fe_pidlock);
  fe_pidlock = NULL;

  /* This function is not expected to return */
  exit(-1);
}

static int
x_error_handler (Display *dpy, XErrorEvent *event)
{
  /* For a couple of not-very-good reasons, we sometimes try to free color
     cells that we haven't allocated, so ignore BadAccess errors from
     XFreeColors().  (This only happens in low color situations when
     substitutions have been performed and something has gone wrong...)
   */
  if (event->error_code == BadAccess &&
      event->request_code == X_FreeColors)
    return 0;

  /* fvwm (a version of twm) tends to issue passive grabs
     on the right mouse button.  This will cause any Motif program to get a
     BadAccess when calling XmCreatePopupMenu(), and will cause it to be
     possible to have popup menus that are still up while no mouse button
     is depressed.  No doubt there's some arcane way to configure fvwm to
     not do this, but it's much easier to just punt on this particular error
     message.
   */
  if (event->error_code == BadAccess &&
      event->request_code == X_GrabButton)
    return 0;

  fprintf (real_stderr, "\n%s:\n", fe_progname);
  XmuPrintDefaultErrorMessage (dpy, event, real_stderr);

#ifndef DONT_PRINT_WIDGETS_IN_ERROR_HANDLER
  {
	  Window window = event ? event->resourceid : None;

	  if (window != None)
	  {
		  Widget guilty = XtWindowToWidget(dpy,window);

		  fprintf(real_stderr,"  Widget hierarchy of resource: ");

		  if (guilty)
		  {
			  while(guilty)
			  {
				  fprintf(real_stderr,"%s",XtName(guilty));

				  guilty = XtParent(guilty);

				  if (guilty)
				  {
					  fprintf(real_stderr,".");
				  }
			  }
		  }
		  else
		  {
			  fprintf(real_stderr,"unknown\n");
		  }

		  fprintf(real_stderr,"\n");
	  }
  }
#endif
  return 0;
}

static void xt_warning_handler(String nameStr, String typeStr,
	String classStr, String defaultStr,
	String* params, Cardinal* num_params)
{
  /* Xt error handler to ignore colormap warnings */
  if (nameStr && *nameStr) {
	/* Ignore all colormap warnings */
    if (!strcmp(nameStr, "noColormap"))
	  return;
  }

  (*old_xt_warning_handler)(nameStr, typeStr, classStr, defaultStr,
			   params, num_params);
}

static void xt_warningOnly_handler(String msg)
{
  static char *ignore_me = NULL;
  int len = 0;

  if (!ignore_me) {
      ignore_me = XP_GetString(XFE_COLORMAP_WARNING_TO_IGNORE);
      if (ignore_me && *ignore_me)
	  len = strlen(ignore_me);
  }

  /* Xt error handler to ignore colormap warnings */
  if (msg && *msg && len && strlen(msg) >= len &&
      !XP_STRNCASECMP(msg, ignore_me, len)) {
      /* Ignore all colormap warning messages */
#ifdef DEBUG_dp
      fprintf(stderr, "dp, I am ignoring \"%s\"\n", msg);
#endif /* DEBUG_dp */
	return;
  }

  if (old_xt_warningOnly_handler && *old_xt_warningOnly_handler)
      (*old_xt_warningOnly_handler)(msg);
}

static void
fe_read_screen_for_rng (Display *dpy, Screen *screen)
{
    XImage *image;
    size_t image_size;
    int32 coords[4];
    int x, y, w, h;

    assert (dpy && screen);
    if (!dpy || !screen) return;

    RNG_GenerateGlobalRandomBytes(coords,  sizeof(coords));

    x = coords[0] & 0xFFF;
    y = coords[1] & 0xFFF;
    w = (coords[2] & 0x7f) | 0x40; /* make sure w is in range [64..128) */
    h = (coords[3] & 0x7f) | 0x40; /* same for h */

#ifdef NSPR_SPLASH
    if (fe_globalData.show_splash)
      PR_XLock();
#endif

    x = (screen->width - w + x) % (screen->width - w);
    y = (screen->height - h + y) % (screen->height - h);
    image = XGetImage(dpy, screen->root, x, y, w, h, ~0, ZPixmap);
    if (!image) return;
    image_size = (image->width * image->height * image->bits_per_pixel) / 8;
    if (image->data)
	RNG_RandomUpdate(image->data, image_size);
    XDestroyImage(image);

#ifdef NSPR_SPLASH
    if (fe_globalData.show_splash)
      PR_XUnlock();
#endif
}

/*****************************
 * Context List manipulation *
 *****************************/

/* XFE maintains a list of all contexts that it created. This is different
   from that maintained by xp as that wouldnt have bookmark, addressbook etc.
   contexts. */

Boolean fe_contextIsValid( MWContext *context )
{
  struct fe_MWContext_cons *cons = fe_all_MWContexts;
  for (; cons && cons->context != context; cons = cons->next);
  return (cons != NULL);
}

void
fe_getVisualOfContext(MWContext *context, Visual **v_ptr, Colormap *cmap_ptr,
				Cardinal *depth_ptr)
{
    Widget mainw = CONTEXT_WIDGET(context);

    while(!XtIsWMShell(mainw) && (XtParent(mainw)!=0))
	mainw = XtParent(mainw);

    XtVaGetValues (mainw,
			XtNvisual, v_ptr,
			XtNcolormap, cmap_ptr,
			XtNdepth, depth_ptr, 0);

    return;
}
/*******************************
 * Session Manager stuff  
 *******************************/
Boolean 
fe_add_session_manager(MWContext *context)
{
	Widget shell;

  	if (context->type != MWContextBrowser &&
        	context->type != MWContextMail &&
        	context->type != MWContextNews) return False;


	someGlobalContext = context;
	shell  = CONTEXT_WIDGET(context);

  	WM_SAVE_YOURSELF = XmInternAtom(XtDisplay(shell),
                "WM_SAVE_YOURSELF", False);
        XmAddWMProtocols(shell,
                                &WM_SAVE_YOURSELF,1);
        XmAddWMProtocolCallback(shell,
                WM_SAVE_YOURSELF,
                fe_wm_save_self_cb, someGlobalContext);
	return True;
}

void fe_wm_save_self_cb(Widget w, XtPointer clientData, XtPointer callData)
{
	Widget shell = w;

	if (save_argv[0])
	  XP_FREE(save_argv[0]);
	save_argv[0]= 0;

	/*always use long name*/
	StrAllocCopy(save_argv[0], fe_progname_long);

	/* Has to register with a valid toplevel shell */
	XSetCommand(XtDisplay(shell),
                XtWindow(shell), save_argv, save_argc);

	/* On sgi: we will get this every 15 mins. So dont ever exit here.
	** The atexit handler fe_atexit_hander() will take care of removing
	** the pid_lock file.
	*/
	fe_MinimalNoUICleanup();
}

Boolean
fe_is_absolute_path(char *filename)
{
	if ( filename && *filename && filename[0] == '/')
		return True;
	return False;
}

Boolean
fe_is_working_dir(char *filename, char** progname)
{
   *progname = 0;

   if ( filename && *filename )
   {
	if ( (int)strlen(filename)>1 && 
		   filename[0] == '.' && filename[1] == '/')
	{
          char *str;
          char *name;

	  name = filename;

	  str = strrchr(name, '/');
	  if ( str ) name = str+1;

	  *progname = (char*)malloc((strlen(name)+1)*sizeof(char));
	  strcpy(*progname, name);

	  return True;
	}
	else if (strchr(filename, '/'))
	{
	  *progname = (char*)malloc((strlen(filename)+1)*sizeof(char));
	  strcpy(*progname, filename);
	  return True;
	}
    }
    return False;
}

Boolean 
fe_found_in_binpath(char* filename, char** dirfile)
{
    char *binpath = 0;
    char *dirpath = 0;
    struct stat buf;

    *dirfile = 0;
    binpath = getenv("PATH");

    if ( binpath )
    {
    	binpath = XP_STRDUP(binpath);
	dirpath = XP_STRTOK(binpath, ":");
        while(dirpath)
        {
	   if ( dirpath[strlen(dirpath)-1] == '/' )
           {
	        dirpath[strlen(dirpath)-1] = '\0';
           }

	   *dirfile = PR_smprintf("%s/%s", dirpath, filename);

 	   if ( !stat(*dirfile, &buf) )
           {
	   	XP_FREE(binpath);
		return True;
           }
	   dirpath = XP_STRTOK(NULL,":");
	   XP_FREE(*dirfile);
	   *dirfile = 0;
        }
	XP_FREE(binpath);
    }
    return False;
}

char *
fe_expand_working_dir(char *cwdfile)
{
   char *dirfile = 0;
   char *string;
#if defined(SUNOS4)||defined(AIX)
   char path [MAXPATHLEN];
#endif

#if defined(SUNOS4)||defined(AIX)
   string = getwd (path);
#else
   string = getcwd(NULL, MAXPATHLEN);
#endif

   dirfile = (char *)malloc((strlen(string)+strlen("/")+strlen(cwdfile)+1) 
			*sizeof(char));
   strcpy(dirfile, string);
   strcat(dirfile,"/");
   strcat(dirfile, cwdfile);
#if !(defined(SUNOS4) || defined(AIX))
   XP_FREE(string);
#endif
   return dirfile;
}

/****************************************
 * This function will return either of these
 * type of the exe file path:
 * 1. return FULL Absolute path if user specifies a full path
 * 2. return FULL Absolute path if the program was found in user 
 *    current working dir 
 * 3. return relative path (ie. the same as it is in fe_progname)
 *    if program was found in binpath
 *
 ****************************************/
const char *
fe_locate_program_path(const char *fe_prog)
{
  char *ret_path = 0;
  char *dirfile = 0;
  char *progname = 0;

  StrAllocCopy(progname, fe_prog);

  if ( fe_is_absolute_path(progname) )
  {
	StrAllocCopy(ret_path, progname);
        XP_FREE(progname);
	return ret_path;
  }
  else if ( fe_is_working_dir(progname, &dirfile) )
  {
	ret_path = fe_expand_working_dir(dirfile);
        XP_FREE(dirfile);
        XP_FREE(progname);
        return ret_path;
  }
  else if ( fe_found_in_binpath(progname, &ret_path) )
  {
        if ( fe_is_absolute_path(ret_path) )
        {
	   /* Found in the bin path; then return only the exe filename */
	   /* Always use bin path as the search path for the file 
	      for consecutive session*/
	   XP_FREE(ret_path);
	   ret_path = progname;
       	   XP_FREE(dirfile);
	}
	else if (fe_is_working_dir(ret_path, &dirfile) )
	{
		XP_FREE(ret_path);
		XP_FREE(progname);
		ret_path = fe_expand_working_dir(dirfile);
        	XP_FREE(dirfile);
	}
	return ret_path;
  }
  else
  {
      XP_FREE(ret_path);
      XP_FREE(dirfile);
      XP_FREE(progname);

      fprintf(stderr,
			  XP_GetString(XFE_MOZILLA_NOT_FOUND_IN_PATH),
			  fe_progname);
    
      exit(-1);
  }
}

/****************************************
 * This function will return either of these
 * type of the file path:
 * 1. return FULL Absolute path if user specifies a full path
 * 2. return FULL Absolute path if the program was found in user 
 *    current working dir 
 * 3. return relative path (ie. the same as it is in fe_progname)
 *    if program was found in binpath
 * 4. return NULL if we cannot locate it
 ****************************************/
static char *
fe_locate_component_path(const char *fe_prog, char *comp_file)
{
  char *ret_path = 0;
  char *dirfile = 0;
  char *progname = 0;
  char  compname[1024];
  char *separator;
  char *moz_home = 0;
  struct stat buf;

  /* replace the program name with component name first */
  XP_STRCPY(compname, fe_prog);
  separator = XP_STRRCHR(compname, '/');
  if (separator) {
	  XP_STRCPY(separator+1, comp_file);
  }
  else {
	  XP_STRCPY(compname, comp_file);
  }

  StrAllocCopy(progname, compname);

  if ( fe_is_absolute_path(progname) ) {
	  if (! stat(progname, &buf)) {
		  StrAllocCopy(ret_path, progname);
		  XP_FREE(progname);
		  return ret_path;
	  }
  }
  
  if ( fe_is_working_dir(progname, &dirfile) ) {
	  ret_path = fe_expand_working_dir(dirfile);
	  if (! stat(ret_path, &buf)) {
		  XP_FREE(dirfile);
		  XP_FREE(progname);
		  return ret_path;
	  }
  }

  /* Now see if we can find it in the bin path */
  if ( fe_found_in_binpath(comp_file, &ret_path) ) {
	  if ( fe_is_absolute_path(ret_path) ) {
		  /* Found in the bin path; then return only the exe filename */
		  /* Always use bin path as the search path for the file 
			 for consecutive session*/
		  XP_FREE(progname);
		  XP_FREE(dirfile);
		  return ret_path;
	  }
	  else if (ret_path[0] == '.' && ret_path[1] == '/') { /* . relative */
		  char* foo = fe_expand_working_dir(&ret_path[2]);
		  XP_FREE(ret_path);
		  ret_path = foo;
		  XP_FREE(progname);
		  XP_FREE(dirfile);
		  return ret_path;
	  }
  }

  /* Check $MOZILLA_HOME */

  moz_home  = getenv("MOZILLA_HOME");

  if (moz_home) {
	  XP_SPRINTF(compname, "%s/%s", moz_home, comp_file);
	  if (! stat(compname, &buf)) {
		  StrAllocCopy(ret_path, compname);
		  if (dirfile) XP_FREE(dirfile);
		  if (progname) XP_FREE(progname);
		  return ret_path;
	  }
  }

  /* cannot find it */

  if (ret_path) XP_FREE(ret_path);
  if (dirfile) XP_FREE(dirfile);
  if (progname) XP_FREE(progname);
  return NULL;
}

/* Retrieve the first entry in the previous session's history list */
XP_Bool fe_ReadLastUserHistory(char **hist_entry_ptr)
{
	char *value;
	char  buffer[1024];
	char *hist_entry = 0;
	FILE *fp = fopen(fe_globalPrefs.user_history_file,"r");

	if ( !fp )
		return FALSE;
 
	if ( !fgets(buffer, 1024, fp) )
		*buffer = 0;

	while (fgets(buffer, 1024, fp )){
		value = XP_StripLine(buffer);
		if (strlen(value)==0 || *value == '#')
			continue;
		hist_entry = XP_STRDUP(value);
		break;
	}
	fclose(fp);
	*hist_entry_ptr = hist_entry;

	if (hist_entry)
		return TRUE;
	else
		return FALSE;
}

void fe_GetProgramDirectory(char *path, int len)
{
	char * separator;
	char * prog = 0;

	*path = '\0';

	if ( fe_is_absolute_path( (char*)fe_progname_long ) )
		strncpy (path, fe_progname_long, len);
	else
	{
		if ( fe_found_in_binpath((char*)fe_progname_long, &prog) )
		{
			strncpy (path, prog, len);
	   		XP_FREE (prog);
		}
	}

	if (( separator = XP_STRRCHR(path, '/') ))
		separator[1] = 0;
	return;
}

static void fe_InitPolarisComponents(char *prog_name)
{
	/* For now, just check if we can find the key file associated with
	 * each Polaris component.
	 */

	fe_calendar_path = fe_locate_component_path(prog_name, "nscal");
	fe_host_on_demand_path = fe_locate_component_path(prog_name, "3270/he3270en.htm");
}

static void fe_InitConference(char *prog_name)
{
	/* For now, just check if we can find the key file associated with
	 * conference
	 */

	fe_conference_path = fe_locate_component_path(prog_name, "nsconference");
}

XP_Bool fe_GetCommandLineDone(void)
{
	return fe_command_line_done;
}

int fe_GetSavedArgc(void)
{
	return save_argc;
}

char ** fe_GetSavedArgv(void)
{
	static int need_to_change_argv_0 = True;

	if (need_to_change_argv_0)
	{
		need_to_change_argv_0 = False;
		
		if (save_argv[0])
		{
			XP_FREE(save_argv[0]);
		}
		
		save_argv[0]= 0;
		
		/*always use long name*/
		StrAllocCopy(save_argv[0], fe_progname_long);
	}

	return save_argv;
}

/*
 * Make Async DNS optional at runtime.
 */
static XP_Bool _use_async_dns = True;

XP_Bool fe_UseAsyncDNS(void)
{
	return _use_async_dns;
}

static void fe_check_use_async_dns(void)
{
	char * c = getenv ("MOZILLA_NO_ASYNC_DNS");
	
	_use_async_dns = True;

	if (c && *c)
	{
		/* Just in case make sure the var is not [fF0] (for funky logic) */
		if (*c != 'f' && *c != 'F' && *c != '0')
		{
			_use_async_dns = False;
		}
	}
}
