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

#include <sys/types.h>
#include <pwd.h>

#include "structs.h"
#include "ntypes.h"
#include "proto.h"
#include "net.h"
#include "plevent.h"
#include "xp.h"
#include "rdf.h"

#include "g-browser-frame.h"

extern char *fe_home_dir;
extern char *fe_config_dir;

gint gnomefe_depth;
GdkVisual *gnomefe_visual;

PRThread *mozilla_thread;
PREventQueue* mozilla_event_queue;

extern const char *XP_AppCodeName;
extern const char *XP_AppVersion;
extern const char *XP_AppPlatform;
const char *XP_Language;

void
fe_url_exit (URL_Struct *url, int status, MWContext *context)
{
  printf ("in fe_url_exit()\n");

  if (status != MK_CHANGING_CONTEXT)
    {
      NET_FreeURLStruct (url);
    }
}

int
net_idle_function(gpointer data)
{
  NET_ProcessNet (NULL, NET_EVERYTIME_TYPE);
  PR_ProcessPendingEvents(mozilla_event_queue);
  return TRUE;
}

int
main(int argc,
     char **argv)
{
  URL_Struct *url;

  MozBrowserFrame *initial_frame;

  XP_AppName = "gnuzilla";
  XP_AppCodeName = "Gnuzilla";
  XP_AppVersion = "0.1";
  XP_Language = "";
  XP_AppPlatform = "";

#ifdef DEBUG
  {
    XP_StatStruct stat_struct;
    
    if(stat(".WWWtraceon", &stat_struct) != -1)
      MKLib_trace_flag = 2;
  }
#endif

  {
    /* initialize NSPR stuff */
#ifdef DEBUG
      extern PRLogModuleInfo* NETLIB;
      NETLIB = PR_NewLogModule("netlib");
#endif

    PR_SetThreadGCAble();
    PR_SetThreadPriority(PR_GetCurrentThread(), PR_PRIORITY_LAST);
    PR_BlockClockInterrupts();

    LJ_SetProgramName(argv[0]);
#if 0
    PR_XLock();
#endif
    mozilla_thread = PR_CurrentThread();
#if 0
    fdset_lock = PR_NewNamedMonitor("mozilla-fdset-lock");
#endif
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

  }

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

  /* Hmm. XFE claims InitNetLib needs to happen before pref init */
  /* The unit for tcp buffer size is changed from kbytes to bytes */
  NET_InitNetLib (8192, 50);

  {
    char buf [1024];
    PR_snprintf (buf, sizeof (buf), "%s/%s", fe_home_dir,
#ifdef OLD_UNIX_FILES
                 ".netscape-preferences"
#else
                 MOZ_USER_DIR "/preferences.js"
#endif
                 );
    
    PREF_Init(buf);
  }

  gnome_init("gnuzilla", NULL, /* argc */ 1, argv, 0, NULL);

  gnomefe_depth = gdk_visual_get_best_depth();
  gnomefe_visual = gdk_visual_get_best_with_depth(gnomefe_depth);

  gtk_widget_set_default_visual(gnomefe_visual);

  PR_UnblockClockInterrupts();

  check_for_lock_file();

  NR_StartupRegistry();
  fe_RegisterConverters ();  

  GH_InitGlobalHistory();

  IL_Init();
  LM_InitMocha ();

  initial_frame = moz_browser_frame_create();

  moz_frame_show(MOZ_FRAME(initial_frame));

  /* Initialize RDF */
  {
    RDF_InitParamsStruct rdf_params = {0, };
    
    rdf_params.profileURL = 
      XP_PlatformFileToURL(fe_config_dir);
    rdf_params.bookmarksURL =
      XP_PlatformFileToURL(fe_GetConfigDirFilename("bookmarks.html"));
    rdf_params.globalHistoryURL = 
      XP_PlatformFileToURL(fe_GetConfigDirFilename("history.db"));
    
    RDF_Init(&rdf_params);
    
    XP_FREEIF(rdf_params.profileURL);
    XP_FREEIF(rdf_params.bookmarksURL);
    XP_FREEIF(rdf_params.globalHistoryURL);
  }
  NPL_Init();
  
  if(argc > 1)
    url = NET_CreateURLStruct(argv[1], NET_NORMAL_RELOAD);
  else
    url = NET_CreateURLStruct("about:blank", NET_NORMAL_RELOAD);


  NET_GetURL(url, FO_CACHE_AND_PRESENT,
             moz_frame_get_context(MOZ_FRAME(initial_frame)),
             fe_url_exit);

  /* XXX is this enough time? */
  gtk_timeout_add(250, net_idle_function, NULL);

  gtk_main();

  /* keep gcc happy */
  return 0;
}
