/* $Id: qtmoz.cpp,v 1.6 1998/10/22 06:04:35 cls%seawood.org Exp $
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

// This is a kludge to get around the fact that DEBUG turns on internal QT
// debugging information that we do not want.

#ifndef DEBUG
#define debug_off
#endif

// Qt includes
#include <qapp.h>
#include <qsocknot.h>
#include <qmsgbox.h>

#ifdef debug_off
#undef DEBUG
#undef debug_off
#endif

#include "client.h"
#include "QtEventPusher.h"
#include "mainwindow.h"
#include "streaming.h"

#include "qtfe_err.h"
#include "plevent.h"
#include "xpgetstr.h"
#include "nf.h"

#include "name.h"
#include "xp.h"
#include "xp_help.h" /* for XP_NetHelp() */
//#include "xp_sec.h"
#include "ssl.h"
#include "np.h"
#include "secnav.h"
#include "secrng.h"
#include "private/prpriv.h"	/* for PR_NewNamedMonitor */

#include "libimg.h"             /* Image Library public API. */

#include "prefapi.h"
#include "hk_funcs.h"

#include "libmocha.h"

#include "prnetdb.h"
#include "plevent.h"

#if 0
#include "bkmks.h"		/* for drag and drop in mail compose */
#endif

#include "NSReg.h"

#define XFE_RDF
#ifdef XFE_RDF
#include "rdf.h"
#endif

#if defined(XP_UNIX)
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
#include <sys/errno.h>
#endif // XP_UNIX

#include <sys/types.h>
#include <fcntl.h>


#ifdef MOZILLA_GPROF
#include "gmon.h"
#endif

#ifndef _STDC_
#define _STDC_ 1
#define HACKED_STDC 1
#endif

#ifdef HACKED_STDC
#undef HACKED_STDC
#undef _STDC_
#endif

#if defined(_CC_MSVC_)
#include <direct.h>
#include <process.h>
#define getcwd _getcwd
#define mkdir  _mkdir
#endif

#if defined(XP_WIN)
extern "C" char *NOT_NULL(const char *p)
{
    ASSERT(p != 0 );
    return (char*)p;
}

extern "C" void XP_AssertAtLine( char *fileName, int line )
{
#ifdef DEBUG
    warning( "Assert failed in %s, line %d", fileName, line );
#endif
}

#endif

#if defined(XP_UNIX) && defined(SW_THREADS)
extern "C" int PR_XGetXtHackFD(void);
#endif
extern "C" Bool QTFE_Confirm(MWContext * context, const char * Msg);

/* This means "suppress the splash screen if there is was a URL specified
   on the command line."  With betas, we always want the splash screen to
   be printed to make sure the user knows it's beta. */
#define NON_BETA

/* Allow a nethelp startup flag */
#define NETHELP_STARTUP_FLAG

const char *fe_progname_long;
const char *fe_progname;
const char *fe_progclass;

char *fe_pidlock = 0;

/* Polaris components */
char *fe_host_on_demand_path = 0;

#if defined(XP_WIN)
typedef int pid_t;
#endif

static int
fe_create_pidlock (const char *name, unsigned long *paddr, pid_t *ppid);

// forwards
static XP_Bool fe_ensure_config_dir_exists ();
static void registerConverters (void);
static const char * fe_locate_program_path(const char *fe_prog);

extern
MWContext *
fe_MakeWindow(QWidget* toplevel, MWContext *context_to_copy,
                        URL_Struct *url, char *window_name, MWContextType type,
                        bool skip_get_url);

XP_Bool fe_ReadLastUserHistory(char **hist_entry_ptr);

PRMonitor *fdset_lock;
extern "C" {
    PRThread *mozilla_thread;
    PREventQueue* mozilla_event_queue;
}

#if defined(NSPR) && defined(TOSHOK_FIXES_THE_SPLASH_SCREEN_HANGS)
#define NSPR_SPLASH
#endif

static void
usage (void)
{
    fprintf (stderr, XP_GetString( QTFE_USAGE_MSG1 ), 0 + 4,
  				fe_progname);

#ifdef USE_NONSHARED_COLORMAPS
    fprintf (stderr,  XP_GetString( QTFE_USAGE_MSG2 ) );
#endif

    fprintf (stderr, XP_GetString( QTFE_USAGE_MSG3 ) );

    fprintf (stderr, XP_GetString( QTFE_USAGE_MSG5 ) );
}

#include <errno.h>
#if 0
#if defined(XP_UNIX)
#if !defined(__FreeBSD__) && !defined(LINUX_GLIBC_2)
#include <sys/errno.h>
extern char *sys_errlist[];
extern int sys_nerr;
#endif
#endif
#endif

void
fe_perror_2 (const char *message)
{
    int e = errno;
    char *es = 0;
    char buf1 [2048];
    char buf2 [512];
    char *b = buf1;
    if (e >= 0 && e < sys_nerr) {
        es = sys_errlist [e];
    } else {
        PR_snprintf (buf2, sizeof (buf2), XP_GetString( QTFE_UNKNOWN_ERROR_CODE ),
                      errno);
        es = buf2;
    }
    if (message)
        PR_snprintf (buf1, sizeof (buf1), "%.900s\n%.900s", message, es);
    else
        b = buf2;
    FE_Alert(0,b);
}

/*******************
 * Signal handlers *
 *******************/
void fe_sigchild_handler(int)
{
#if defined(XP_UNIX)
    pid_t pid;
    int status = 0;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
#ifdef DEBUG_dp
      fprintf(stderr, "fe_sigchild_handler: Reaped pid %d.\n", pid);
#endif
    }
#endif
}

/* Netlib re-enters itself sometimes, so this has to be a counter not a flag */
static int fe_netlib_hungry_p = 0;

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

/* ******************************************* Windows ifdef */

#if defined(XP_WIN)
extern "C"
void FE_Trace( const char *s )
{
    // QMessageBox::information( 0, "FE_Trace", s );
}

extern "C"
void FE_Print( const char *s )
{
    // QMessageBox::information( 0, "FE_Print", s );
    //    debug("FE_Print called, no implementation");
}

extern "C"
void NPL_Init()
{
    // QMessageBox::information( 0, "", "NPL_Init" );
}

extern "C"
void NPL_PreparePrint( MWContext *, SHIST_SavedData *)
{
}


extern "C" void
NPL_DeleteSessionData(MWContext*, void*)
{
}

extern "C" char*
NPL_FindPluginEnabledForType(const char*)
{
    return 0;
}

extern "C" void
NPL_EmbedURLExit(URL_Struct *, int, MWContext *)
{
}

extern "C" void
NPL_SameElement(LO_EmbedStruct*)
{
}

extern "C" void
NPL_SetPluginWindow(void *)
{
}

extern "C" NPError
NPL_RefreshPluginList(XP_Bool)
{
    return NPERR_NO_ERROR;
}

extern "C" NPBool
NPL_IteratePluginFiles(NPReference*, char**, char**, char**)
{
    return FALSE;
}

extern "C" NPBool
NPL_IteratePluginTypes(NPReference*, NPReference, NPMIMEType*, char***,
		       char**, void**)
{
    return (NPBool)FALSE;
}

void
NPL_URLExit(URL_Struct *, int, MWContext *)
{
}

extern XP_Bool
NPL_HandleURL(MWContext *, FO_Present_Types, URL_Struct *,
	      Net_GetUrlExitFunc *)
{
    return (XP_Bool)FALSE;
}


extern "C"
void
NET_RegisterConverters (char *, char *)
{
}

extern "C"
void XL_TranslatePostscript(MWContext *, URL_Struct *, SHIST_SavedData *_data, PrintSetup *)
{
}

extern "C"
void XL_InitializePrintSetup(PrintSetup *)
{
}

extern "C" int
WFE_DoCompuserveAuthenticate(MWContext *, URL_Struct *, char * )
{
    return 0;
}

extern "C" char *
WFE_BuildCompuserveAuthString(URL_Struct *)
{
    return 0;
}

extern "C"
PRBool FE_IsNetscapeDefault()
{
    // QMessageBox::information( 0, "", "FE_ISNETSCAPEDEFAULT" );
    return PR_TRUE;
}

extern "C"
PRBool FE_MakeNetscapeDefault()
{
    // QMessageBox::information( 0, "", "FE_MAKENETSCAPEDEFAULT" );
    return PR_TRUE;
}

extern "C"
void FE_URLEcho(URL_Struct *, int, MWContext *)
{
    //    QMessageBox::information( 0, "", "FE_URLECHO" );
}

extern "C"
int32 FE_SystemRAM( void )
{
    // QMessageBox::information( 0, "", "FE_SYSTEMRAM" );
    return 640000;
}

extern "C"
int32 FE_SystemClockSpeed( void )
{
    // QMessageBox::information( 0, "", "FE_SYSTEMCLOCKSPEED" );
    return 8;
}

extern "C"
char *FE_SystemCPUInfo(void)
{
    // QMessageBox::information( 0, "", "FE_SYSTEMCPUINFO" );
    return "8088";
}

extern "C"
void FE_LoadUrl( char *, BOOL )
{
    // QMessageBox::information( 0, "", "FE_LOADURL" );
    //    debug("FE_LoadUrl");
}

extern "C"
void FE_UrlChangedContext(URL_Struct *, MWContext *, MWContext *)
{
    // QMessageBox::information( 0, "", "FE_URLCHANGEDCONTEXT" );
}


extern "C"
XP_Bool FE_UseExternalProtocolModule(MWContext *, FO_Present_Types,
				     URL_Struct *,
				     Net_GetUrlExitFunc *)
{
    //    QMessageBox::information( 0, "", "FE_USEEXTERNALPROTOCOLMODULE" );
    return (XP_Bool)FALSE;
}

extern "C"
int32 FE_LPtoDPoint(MWContext *, int32 logicalPoint)
{
    // QMessageBox::information( 0, "", "FE_LPTODPOINT" );
    return logicalPoint;
}

extern "C"
void *FE_GetSingleByteTable(int16, int16, int)
{
    // QMessageBox::information( 0, "", "FE_GETSINGLEBYTETABLE" );
    return 0;
}

extern "C"
char *FE_LockTable(void **)
{
    // QMessageBox::information( 0, "", "FE_LOCKTABLE" );
    return 0;
}

extern "C"
void FE_FreeSingleByteTable(void **)
{
    // QMessageBox::information( 0, "", "FE_FREESINGLEBYTETABLE" );
}

extern "C"
void FE_FinishedRelayout(MWContext *)
{
    // QMessageBox::information( 0, "", "FE_FINISHEDRELAYOUT" );
}

extern "C"
void FE_DisplayDropTableFeedback(MWContext *, EDT_DragTableData *)
{
    // QMessageBox::information( 0, "", "FE_DISPLAYDROPTABLEFEEDBACK" );
}

extern "C"
void FE_UpdateEnableStates(MWContext *)
{
    // QMessageBox::information( 0, "", "FE_UPDATEENABLESTATES" );
}

static char fixFont[12] = "Courier New";
static char	propFont[6] = "Arial";

extern "C" XP_Bool FE_CheckFormTextAttributes(MWContext *context,
                LO_TextAttr *oldAttr, LO_TextAttr *newAttr, int32 type)
{
    if (oldAttr && newAttr && !oldAttr->font_face)  {
	memcpy(newAttr,oldAttr, sizeof(LO_TextAttr));
	// setup the text attribute here.


	if (oldAttr->fontmask & LO_FONT_FIXED) {
	    newAttr->font_face = fixFont;
	}
	else {
	    if (newAttr->size > 1);
	    newAttr->size -=1;
	    newAttr->font_face = propFont;
	}
	newAttr->font_weight = 500;  /* 100, 200, ... 900 */
	newAttr->FE_Data = NULL;     /* For the front end to store font IDs */
	return TRUE;
    }
    else
	return FALSE;
	
#if 0
    QMessageBox::information( 0, "", "FE_CHECKFORMTEXTATTRIBUTES" );
    return (XP_Bool)TRUE;
#endif
}

extern "C"
void BMFE_ChangingBookmarksFile()
{
}

extern "C"
void BMFE_ChangedBookmarksFile()
{
}


#endif // XP_WIN
/* ******************************************* Windows ifdef */

extern "C"
void FEU_StayingAlive()
{
    // QMessageBox::information( 0, "", "FEU_STAYINGALIVE" );
}

extern "C" void FE_AlternateCompose(
    char *, char *, char *, char *, char *,
    char *, char *, char *,
    char *, char *, char *,
    char *, char *,
    char *, char *, char *)
{
    // QMessageBox::information( 0, "", "FE_ALTERNATECOMPOSE" );
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
#if defined(XP_UNIX)
    int fd0 = open ("/dev/null", O_RDWR);
    int fd1 = open ("/dev/null", O_RDWR);
    int fd2 = open ("/dev/null", O_RDWR);
    if (fd0 > 2U) close (fd0);
    if (fd1 > 2U) close (fd1);
    if (fd2 > 2U) close (fd2);
#endif
}

#if defined(XP_UNIX)
extern "C" char **environ;
#endif

static void
losePrivilege(void)
{
  /* If we've been run as setuid or setgid to someone else (most likely root)
     turn off the extra permissions.  Nobody ought to be installing Mozilla
     as setuid in the first place, but let's be extra special careful...
     Someone might get stupid because of movemail or something.
  */
#if defined(XP_UNIX)
  setgid (getgid ());
  setuid (getuid ());
#endif
  /* Is there anything special we should do if running as root?
     Features we should disable...?
     if (getuid () == 0) ...
  */
}

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
    strncpy(buf, "fe_version", totlen - 1);
    totlen -= strlen(buf);
    strncat(buf, versionLocale, totlen - 1);

    XP_AppVersion = strdup (buf);
    /* if it fails, leave XP_AppVersion NULL */
}

extern const char fe_version[];

/*
 * build_user_agent_string
 */
static void
build_user_agent_string(char *versionLocale)
{
    char buf [1024];

#if defined(XP_UNIX)
    struct utsname uts;

    strcpy (buf, fe_version);

#ifdef GOLD
    strcat(buf, "Gold");
#endif

    if (versionLocale) {
	strcat (buf, versionLocale);
    }

    strcat (buf, " (Qt; ");
#if defined(_WS_WIN_)
    strcat (buf, "Windows; ");
#elif defined(_WS_X11_)
    strcat (buf, "X11; ");
#endif
//    strcat (buf, XP_SecurityVersion (FALSE));
    strcat (buf, "; ");

    if (uname (&uts) < 0) {
#if defined(__sun)
	strcat (buf, "SunOS");
#elif defined(__NT__)
	strcat (buf, "Windows NT");
#elif defined(_WS_WIN_)
	strcat (buf, "Windows");
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
	strcat (buf, "Linux");
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
        strcat (buf,XP_GetString(QTFE_MOZILLA_UNAME_FAILED));

	fprintf (stderr,
		 XP_GetString(QTFE_MOZILLA_UNAME_FAILED_CANT_DETERMINE_SYSTEM),
		 fe_progname);
    } else {
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

#elif defined(XP_WIN)
    strcpy (buf, "fe_version (Windows; I; Windows/Qt; Nav" );
#endif

    strcat (buf, ")");

#if defined(__sun) && !defined(__svr4__)	/* compiled on SunOS 4.1.3 */
    if (uts.release [0] == '5') {
	fprintf (stderr,
		 XP_GetString(QTFE_MOZILLA_TRYING_TO_RUN_SUNOS_ON_SOLARIS),
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


QtEventPusher::QtEventPusher(int fd)
{
    QSocketNotifier* sn = new QSocketNotifier( fd, QSocketNotifier::Read, this );
    QObject::connect( sn, SIGNAL(activated(int)), this, SLOT(push(int)));
}
void QtEventPusher::push(int fd)
{
    debug( "Push %i", fd );
    PR_ProcessPendingEvents(mozilla_event_queue);
    NET_ProcessNet(0,NET_EVERYTIME_TYPE);
    NET_PollSockets();
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

 this is installed as a qt post-routine.

 */
static void
minimalNoUICleanup()
{
    if (fe_pidlock) unlink (fe_pidlock);
        fe_pidlock = NULL;

    //fe_SaveBookmarks ();

    PREF_SavePrefFile();
    NR_ShutdownRegistry();

    RDF_Shutdown();
    GH_SaveGlobalHistory ();
    NET_SaveCookies(NULL);
    //AltMailExit();
}


static const int  hist_buflen = 1024;
static char hist_buffer[hist_buflen];

void
mozilla_main(int argc, char** argv)
{
    int i;
#if defined(XP_UNIX)
    struct sigaction act;
#endif

    char *s;

    char versionLocale[32];

    versionLocale[0] = 0;

    fe_progname = argv [0];
    s = XP_STRRCHR (fe_progname, '/');
    if (s) fe_progname = s + 1;

    losePrivilege();	/* Do this real early */

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
#if defined(XP_UNIX) && !defined(NDEBUG)
    extern PRLogModuleInfo* NETLIB;
    NETLIB = PR_NewLogModule("netlib");
#endif

    PR_SetThreadGCAble();
    PR_SetThreadPriority(PR_GetCurrentThread(), PR_PRIORITY_LAST);
    PR_BlockClockInterrupts();

#if defined(XP_UNIX)
    PR_XLock();
#endif
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

    if (mozilla_event_queue == NULL) {
        fprintf(stderr,
	      XP_GetString(QTFE_MOZILLA_FAILED_TO_INITIALIZE_EVENT_QUEUE),
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

#if defined(XP_UNIX) && defined(DEBUG)
    XP_StatStruct stat_struct;

    extern int il_debug;
    if(getenv("ILD"))
        il_debug=atoi(getenv("ILD"));

    if(stat(".WWWtraceon", &stat_struct) != -1)
        MKLib_trace_flag = 2;
#endif

    /* user agent stuff used to be here */
    fe_progclass = QTFE_PROGCLASS_STRING;
    XP_AppName = (char *)fe_progclass;
    XP_AppCodeName = "Mozilla";

#if defined(XP_WIN)
#define OSTYPE "WIN32"
#endif
    /* OSTYPE is defined by the build environment (config.mk) */
    XP_AppPlatform = OSTYPE;

    /* Hack the -help and -version arguments before opening the display. */
    for (i = 1; i < argc; i++) {
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
	    fprintf (stderr, "Qt scape\n");
	    exit (0);
        }
        else if (!XP_STRCASECMP(argv [i], "-netcaster") ||
                 !XP_STRCASECMP(argv [i], "--netcaster")
                 )
        {
            char *r = argv [i];
            if (r[0] == '-' && r[1] == '-')
              r++;
        }
    }

#if defined(__sun)
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
    } else {
	char *try1 = "/usr/lib/X11/XKeysymDB";
	char *try2 = "/usr/openwin/lib/XKeysymDB";
	char *try3 = "/usr/openwin/lib/X11/XKeysymDB";
	char buf [1024];
	if (override) {
	    fprintf (stderr,
				 XP_GetString(QTFE_MOZILLA_XKEYSYMDB_SET_BUT_DOESNT_EXIST),
				 argv [0], override);
	}

        if (!stat (try1, &st)) {
	    PR_snprintf (buf, sizeof (buf), "XKEYSYMDB=%.900s", try1);
	    putenv (buf);
	} else if (!stat (try2, &st)) {
	    PR_snprintf (buf, sizeof (buf), "XKEYSYMDB=%.900s", try2);
	    putenv (buf);
	} else if (!stat (try3, &st)) {
	    PR_snprintf (buf, sizeof (buf), "XKEYSYMDB=%.900s", try3);
	    putenv (buf);
	} else {
	    fprintf (stderr,
		     XP_GetString(QTFE_MOZILLA_NO_XKEYSYMDB_FILE_FOUND),
		     argv [0], try1, try2, try3);
	}
    }
#endif /* sun */

    /* Must do this early now so that mail can load directories */
    /* Setting fe_home_dir likewise must happen early. */

    char buf [1024];
#if defined(XP_WIN)
    PREF_Init("c:/qtmoz/preferences.js");
    fe_home_dir = "c:/qtmoz";
#else
    fe_home_dir = getenv ("HOME");
    if (!fe_home_dir || !*fe_home_dir)
    {
        /* Default to "/" in case a root shell is running in dire straits. */
        struct passwd *pw = getpwuid(getuid());

        fe_home_dir = pw ? pw->pw_dir : "/";
    } else {
      char *slash;

      /* Trim trailing slashes just to be fussy. */
      while ((slash = strrchr(fe_home_dir, '/')) && slash[1] == '\0')
	*slash = '\0';
    }

    PR_snprintf (buf, sizeof (buf), "%s/%s", fe_home_dir,
        MOZ_USER_DIR "/preferences.js");
    PREF_Init((char*)buf);
#endif // XP_WIN

#if defined(XP_WIN)
    fe_progname_long = argv[0];	/* Qt knows where the program is */
#else
    fe_progname_long = fe_locate_program_path (argv [0]);
#endif

    PR_UnblockClockInterrupts();

    int fd;

    /*
     * We would like to yield the X lock to other (Java) threads when
     * the Mozilla thread calls select(), but we should only do so
     * when Qt calls select from its main event loop. (There are other
     * times when select is called, e.g. from within Xlib. At these
     * times, it is not safe to release the X lock.) Within select,
     * we detect that the caller is Qt by looking at the file
     * descriptors. If one of them is this special unused FD that we
     * are adding here, then we know that Qt is calling us, so it is
     * safe to release the X lock. -- erik
     */
#if defined(XP_UNIX) && defined(SW_THREADS)
    if ( (fd = PR_XGetXtHackFD()) >= 0 )
	new QSocketNotifier( fd, QSocketNotifier::Read, qApp );
#endif
    if ( (fd = PR_GetEventQueueSelectFD(mozilla_event_queue)) >= 0 )
	new QtEventPusher(fd);

    /* Things blow up if argv[0] has a "." in it. */
    s = (char *) fe_progname;
    while ((s = strchr (s, '.')))
      *s = '_';

    /* Hack remaining arguments - assume things beginning with "-" are
     * misspelled switches, instead of file names.  Magic for -help and
     * -version has already happened.
     */
    for (i = 1; i < argc; i++)
        if (*argv [i] == '-') {
	    fprintf (stderr, XP_GetString( QTFE_UNRECOGNISED_OPTION ),
		     fe_progname, argv [i]);
	    usage ();
	    exit (-1);
        }

    /* See if another instance is running.  If so, don't use the .db files */
    if (fe_ensure_config_dir_exists ())
    {
        char *name;
        unsigned long addr;
        pid_t pid;

        name = PR_smprintf ("%s/lock", fe_config_dir);
        addr = 0;
        pid = 0;
        if (name == NULL)
	    /* out of memory -- what to do? */;
        else if (fe_create_pidlock (name, &addr, &pid) == 0) {
	    /* Remember name in fe_pidlock so we can unlink it on exit. */
	    fe_pidlock = name;
	} else {
	    char *fmt = NULL;
	    char *lock = name ? name : MOZ_USER_DIR "/lock";

            fmt = PR_sprintf_append(fmt, XP_GetString(QTFE_APP_HAS_DETECTED_LOCK),
				    XP_AppName, lock);
	    fmt = PR_sprintf_append(fmt,
				    XP_GetString(QTFE_ANOTHER_USER_IS_RUNNING_APP),
				    XP_AppName,
				    fe_config_dir);

	    if (addr != 0 && pid != 0) {
	        struct in_addr inaddr;
	        PRHostEnt *hp, hpbuf;
	        char dbbuf[PR_NETDB_BUF_SIZE];
	        const char *host;

                inaddr.s_addr = addr;

	        PRStatus sts;
	        PRNetAddr pr_addr;

	        pr_addr.inet.family = AF_INET;
	        pr_addr.inet.ip = addr;
	        sts = PR_GetHostByAddr(&pr_addr,dbbuf, sizeof(dbbuf), &hpbuf);
	        if (sts == PR_FAILURE)
		    hp = NULL;
	        else
		    hp = &hpbuf;
	        host = (hp == NULL) ? inet_ntoa(inaddr) : hp->h_name;
	        fmt = PR_sprintf_append(fmt,
		        XP_GetString(QTFE_APPEARS_TO_BE_RUNNING_ON_HOST_UNDER_PID),
		        host, (unsigned)pid);
	    }

	    fmt = PR_sprintf_append(fmt,
				  XP_GetString(QTFE_YOU_MAY_CONTINUE_TO_USE),
				  XP_AppName);
	    fmt = PR_sprintf_append(fmt,
				  XP_GetString(QTFE_OTHERWISE_CHOOSE_CANCEL),
				  XP_AppName, lock, XP_AppName);

            if (fmt) {
	        if (!QTFE_Confirm(0,fmt))
		    exit (0);
	        free (fmt);
	    }
	    if (name) free (name);

	    /* Keep on-disk databases from being open-able. */
	    dbSetOrClearDBLock (LockOutDatabase);
	}
    }

#if defined(XP_UNIX)
    /* Install the default signal handlers */
    act.sa_handler = fe_sigchild_handler;
    act.sa_flags = 0;
    sigfillset(&act.sa_mask);
    sigaction (SIGCHLD, &act, NULL);
#endif

    /* New XP-prefs routine. */
    SECNAV_InitConfigObject();

    PR_snprintf (buf, sizeof (buf), "%s/%s", fe_home_dir, MOZ_USER_DIR "/user.js");
    PREF_ReadUserJSFile(buf);
    PR_snprintf (buf, sizeof (buf), "%s/%s", fe_home_dir, MOZ_USER_DIR "/hook.js");
    HK_ReadHookFile(buf);

    /*fe_globalPrefs.global_history_expiration*/
    GH_SetGlobalHistoryTimeout (14 * 24 * 60 * 60);

    NR_StartupRegistry();

    /* this must be before InstallPreferences(),
       and after fe_InitializeGlobalResources(). */
    registerConverters ();

#ifdef SAVE_ALL
    SAVE_Initialize ();	     /* Register some more converters. */
#endif /* SAVE_ALL */

#if defined(XP_WIN)
    fe_config_dir = "c:/qtmoz";
    mkdir("c:/qtmoz");
#endif
    PREF_SetDefaultCharPref("profile.name", "hackers@troll.no");
    PREF_SetDefaultCharPref("profile.directory", fe_config_dir);
    PREF_SetDefaultIntPref("profile.numprofiles", 1);


    /* SECNAV_INIT needs this defined, but build_user_agent_string cannot
     * be called until after SECNAV_INIT, so call this simplified version.
     */
    build_simple_user_agent_string(versionLocale);

    // Init global history database
    PREF_GetCharPref("browser.history_file", hist_buffer, &hist_buflen);
    FE_GlobalHist = hist_buffer;
    GH_InitGlobalHistory();

    /*
    ** Initialize the security library.
    **
    ** MAKE SURE THIS IS DONE BEFORE YOU CALL THE NET_InitNetLib, CUZ
    ** OTHERWISE THE FILE LOCK ON THE HISTORY DB IS LOST!
    */
    SECNAV_Init();

    /* Must be called after ekit, since user agent is configurable */
    /* Must also be called after SECNAV_Init, since crypto is dynamic */
    build_user_agent_string(versionLocale);

    RNG_SystemInfoForRNG ();
    SECNAV_RunInitialSecConfig();

    /* Initialize the network library. */
    /* The unit for tcp buffer size is changed from ktypes to btyes */
    NET_InitNetLib (8192, 50);


    /* Initialize the Image Library */
    IL_Init();

    /* Initialize libmocha after netlib and before plugins. */
    LM_InitMocha ();


#ifdef XFE_RDF
    /* Initialize RDF */
    {
       RDF_InitParamsStruct rdf_params;
       char *buf = 0; 
       rdf_params.profileURL 
          = XP_PlatformFileToURL(fe_config_dir);
       
       if (PREF_CopyCharPref( "browser.bookmark_file", &buf ) == PREF_OK ) 
          rdf_params.bookmarksURL = XP_PlatformFileToURL(buf);
       
       rdf_params.globalHistoryURL
          = XP_PlatformFileToURL(hist_buffer);
       
       RDF_Init(&rdf_params);
       
       XP_FREEIF(rdf_params.profileURL);
       XP_FREEIF(rdf_params.bookmarksURL);
       XP_FREEIF(rdf_params.globalHistoryURL);
       free(buf);
   }
#endif /*XFE_RDF*/

    NPL_Init();

    /* At this point, everything in argv[] is a document for us to display.
     Create a window for each document.
    */
    if (argc > 1) {
	for (i = 1; i < argc; i++) {
	    char buf [2048];
	    if (argv [i][0] == '/') {
		/* It begins with a / so it's a file. */
		URL_Struct *url;

		PR_snprintf (buf, sizeof (buf), "file:%.900s", argv [i]);
	        url = NET_CreateURLStruct (buf, NET_DONT_RELOAD);

		FE_MakeNewWindow (0, url, NULL, 0);
	    } else {
	        PRHostEnt hpbuf;
		char dbbuf[PR_NETDB_BUF_SIZE];
		char *s = argv [i];

		while (*s && *s != ':' && *s != '/')
		    s++;

	        if (*s == ':' || (PR_GetHostByName (argv [i], dbbuf, sizeof(dbbuf), &hpbuf) == PR_SUCCESS)) {
		    /* There is a : before the first / so it's a URL.
		       Or it is a host name, which are also magically URLs.  */
		    URL_Struct *url = NET_CreateURLStruct (argv [i], NET_DONT_RELOAD);
		    FE_MakeNewWindow (0, url, NULL, 0);
		} else {
		    /* There is a / or end-of-string before so it's a file. */
		    char cwd [1024];
		    URL_Struct *url;
#ifdef SUNOS4
		    if (! getwd (cwd))
#else
		    if (! getcwd (cwd, sizeof cwd))
#endif
		    {
			fprintf (stderr, "%s: getwd: %s\n", fe_progname, cwd);
			break;
		    }
		    PR_snprintf (buf, sizeof (buf),
			"file:%.900s/%.900s", cwd, argv [i]);
		    url = NET_CreateURLStruct (buf, NET_DONT_RELOAD);
		    FE_MakeNewWindow (0, url, NULL, 0);
		}
	    }
	} /* end for */
    } else {
        FE_MakeNewWindow (0, 0, NULL, 0);
    }
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

#if 0
#if defined(XP_UNIX)
#if !defined(__FreeBSD__) && !defined(MACLINUX) && !defined(LINUX_GLIBC_2)
extern char *sys_errlist[];
extern int sys_nerr;
#endif
#endif
#endif

static XP_Bool
fe_ensure_config_dir_exists ()
{
    char *dir, *fmt;
    static char buf [2048];
    struct stat st;
    XP_Bool exists;

    dir = PR_smprintf ("%s/%s", fe_home_dir, MOZ_USER_DIR);
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
        if (rename (dir, loser) == 0) {
	    fmt = XP_GetString( QTFE_EXISTED_BUT_WAS_NOT_A_DIRECTORY );
	    exists = FALSE;
	} else {
	  fmt = XP_GetString( QTFE_EXISTS_BUT_UNABLE_TO_RENAME );
	}

	PR_snprintf (buf, sizeof (buf), fmt, XP_AppName, dir, loser);
        XP_FREE (loser);
        FE_Alert (0,buf);
        if (exists) {
	  free (dir);
	  return FALSE;
	}
    }

    if (!exists) {
        /* ~/.netscape/ does not exist.  Create the directory.
         */
#if defined(XP_WIN)
        if (mkdir (dir) < 0) {
#else
        if (mkdir (dir, 0700) < 0) {
#endif
	    fmt = XP_GetString( QTFE_UNABLE_TO_CREATE_DIRECTORY );
	    PR_snprintf (buf, sizeof (buf), fmt, XP_AppName, dir,
		         ((errno >= 0 && errno < sys_nerr)
		          ? sys_errlist [errno] : XP_GetString( QTFE_UNKNOWN_ERROR )));
	    FE_Alert (0,buf);
	    free (dir);
	    return FALSE;
        }
    }

    fe_config_dir = dir;
    return TRUE;
}

#ifndef INADDR_LOOPBACK
#define	INADDR_LOOPBACK		0x7F000001
#endif

#define MAXTRIES	100

void
quitQapp(int status)
{
    if (qApp) qApp->exit(status); // Will actually exit next loop
}

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

    myaddr = inaddr.s_addr;
    signature = PR_smprintf ("%s:%u", inet_ntoa (inaddr), (unsigned)getpid ());
    tries = 0;
    addr = 0;
    pid = 0;
#if defined(XP_WIN)
    rval = 0;
#elif defined(XP_UNIX)
    while ((rval = symlink (signature, name)) < 0)
    {
        if (errno != EEXIST)
	    break;
        len = readlink (name, buf, sizeof buf - 1);
        if (len > 0) {
	    buf[len] = '\0';
	    colon = strchr (buf, ':');
	    if (colon != NULL) {
	        *colon++ = '\0';
	        if ((addr = inet_addr (buf)) != (unsigned long)-1) {
		    pid = strtol (colon, &after, 0);
		    if (pid != 0 && *after == '\0') {
		        if (addr != myaddr) {
			    /* Remote pid: give up even if lock is stuck. */
			    break;
		        }

		        /* signator was a local process -- check liveness */
		        if (kill (pid, 0) == 0 || errno != ESRCH) {
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
#endif

    if (rval == 0)
    {
#if defined(XP_UNIX)
        struct sigaction act, oldact;

        act.sa_handler = quitQapp;
        act.sa_flags = 0;
        sigfillset (&act.sa_mask);

        /* Set SIGINT, SIGTERM and SIGHUP to our quitQapp(). If these signals
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

#ifdef SUNOS4
        /* atexit() is not available in sun4. Use qt's support. */
	qAddPostRoutine( minimalNoUICleanup );
#else
        /* Register a atexit() handler to remove lock file */
        atexit(minimalNoUICleanup);
#endif /* SUNOS4 */

#endif // XP_UNIX
    }
    free (signature);
    *paddr = addr;
    *ppid = pid;
    return rval;
}

XP_Bool
fe_is_absolute_path(char *filename)
{
    return ( filename && *filename && filename[0] == '/' );
}

XP_Bool
fe_is_working_dir(char *filename, char** progname)
{
    *progname = 0;

    if ( filename && *filename ) {
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

	    return TRUE;
	} else if (strchr(filename, '/')) {
	    *progname = (char*)malloc((strlen(filename)+1)*sizeof(char));
	    strcpy(*progname, filename);
	    return TRUE;
	}
    }
    return FALSE;
}

XP_Bool
fe_found_in_binpath(char* filename, char** dirfile)
{
    char *binpath = 0;
    char *dirpath = 0;
    struct stat buf;

    *dirfile = 0;
    binpath = getenv("PATH");

    if ( binpath ) {
    	binpath = XP_STRDUP(binpath);
	dirpath = XP_STRTOK(binpath, ":");
        while(dirpath) {
	    if ( dirpath[strlen(dirpath)-1] == '/' ) {
	        dirpath[strlen(dirpath)-1] = '\0';
            }

	    *dirfile = PR_smprintf("%s/%s", dirpath, filename);

	    if ( !stat(*dirfile, &buf) ) {
	   	XP_FREE(binpath);
		return TRUE;
            }
	    dirpath = XP_STRTOK(NULL,":");
	    XP_FREE(*dirfile);
	    *dirfile = 0;
        }
	XP_FREE(binpath);
    }
    return FALSE;
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
static const char *
fe_locate_program_path(const char *fe_prog)
{
    char *ret_path = 0;
    char *dirfile = 0;
    char *progname = 0;

    StrAllocCopy(progname, fe_prog);

    if ( fe_is_absolute_path(progname) ) {
	StrAllocCopy(ret_path, progname);
        XP_FREE(progname);
	return ret_path;
    } else if ( fe_is_working_dir(progname, &dirfile) ) {
	ret_path = fe_expand_working_dir(dirfile);
        XP_FREE(dirfile);
        XP_FREE(progname);
        return ret_path;
    } else if ( fe_found_in_binpath(progname, &ret_path) ) {
        if ( fe_is_absolute_path(ret_path) ) {
	    /* Found in the bin path; then return only the exe filename */
	    /* Always use bin path as the search path for the file
	       for consecutive session*/
	    XP_FREE(ret_path);
	    ret_path = progname;
       	    XP_FREE(dirfile);
	} else if (fe_is_working_dir(ret_path, &dirfile) ) {
	    XP_FREE(ret_path);
	    XP_FREE(progname);
	    ret_path = fe_expand_working_dir(dirfile);
	    XP_FREE(dirfile);
	}
	return ret_path;
    } else {
        XP_FREE(ret_path);
        XP_FREE(dirfile);
        XP_FREE(progname);

	fprintf(stderr,
			  XP_GetString(QTFE_MOZILLA_NOT_FOUND_IN_PATH),
			  fe_progname);

	exit(-1);
    }
    return "";
}


/* Retrieve the first entry in the previous session's history list */
XP_Bool fe_ReadLastUserHistory(char **hist_entry_ptr)
{
    char *value;
    char  buffer[1024];
    char *hist_entry = 0;
    FILE *fp = fopen("qtmozilla_history","r");

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

// Called in modules/libpref/src/prefapi.c
extern "C"
void fe_GetProgramDirectory(char *path, int len)
{
    char * separator;
    char * prog = 0;

    *path = '\0';

    if ( fe_is_absolute_path( (char*)fe_progname_long ) )
	strncpy (path, fe_progname_long, len);
    else if ( fe_found_in_binpath((char*)fe_progname_long, &prog) ) {
	strncpy (path, prog, len);
	XP_FREE (prog);
    }

    if (( separator = XP_STRRCHR(path, '/') ))
	separator[1] = 0;
}


static void myMessageOutput( QtMsgType, const char * )
{
}


class NetProcess : public QObject {
public:
    NetProcess() { startTimer(50); }
    void timerEvent(QTimerEvent*) {
	NET_ProcessNet(0,NET_EVERYTIME_TYPE);
        PR_ProcessPendingEvents(mozilla_event_queue);
	NET_PollSockets();
    }
};

main(int argc, char** argv)
{
#ifdef NDEBUG
    qInstallMsgHandler( myMessageOutput );
#endif
    QApplication::setFont(QFont("Helvetica", 12));
    QApplication::setColorSpec(QApplication::ManyColor);
    QApplication app(argc, argv);
    QObject::connect( &app, SIGNAL( lastWindowClosed() ),
		      &app, SLOT( quit() ) );
    mozilla_main(argc, argv);
    NetProcess np;
    return app.exec();
}

static
void
registerConverters (void)
{
#ifdef NEW_DECODERS
    NET_ClearAllConverters ();
#endif /* NEW_DECODERS */
    NF_FontBrokerInitialize();

    /* Register standard decoders
       This must come AFTER all calls to NET_RegisterExternalDecoderCommand(),
       (at least in the `NEW_DECODERS' world.)
     */
    NET_RegisterMIMEDecoders ();

    /* How to save to disk. */
    NET_RegisterContentTypeConverter( "*", FO_SAVE_AS, NULL,
				      fe_MakeSaveAsStream );

    /* Saving any binary format as type `text' should save as `source' instead.
     */
    NET_RegisterContentTypeConverter( "*", FO_SAVE_AS_TEXT, NULL,
				      fe_MakeSaveAsStreamNoPrompt );
    NET_RegisterContentTypeConverter( "*", FO_QUOTE_MESSAGE, NULL,
				      fe_MakeSaveAsStreamNoPrompt );

    /* default presentation converter - offer to save unknown types. */
    NET_RegisterContentTypeConverter( "*", FO_PRESENT, NULL,
				      fe_MakeSaveAsStream );

#if 0
    NET_RegisterContentTypeConverter( "*", FO_VIEW_SOURCE, NULL,
				      fe_MakeViewSourceStream );
#endif

#ifndef NO_MOCHA_CONVERTER_HACK
    /* libmocha:LM_InitMocha() installs this convert. We blow away all
     * converters that were installed and hence these mocha default converters
     * dont get recreated. And mocha has no call to re-register them.
     * So this hack. - dp/brendan
     */
    NET_RegisterContentTypeConverter(APPLICATION_JAVASCRIPT, FO_PRESENT, 0,
				     NET_CreateMochaConverter);
#endif /* NO_MOCHA_CONVERTER_HACK */

    /* Parse stuff out of the .mime.types and .mailcap files.
     * We dont have to check dates of files for modified because all that
     * would have been done by the caller. The only place time checking
     * happens is
     * (1) Helperapp page is created
     * (2) Helpers are being saved (OK button pressed on the General Prefs).
     */
#define THE_SIZE 1024
    int size = THE_SIZE;
    char buf1[THE_SIZE];
    char buf2[THE_SIZE];
    PREF_GetCharPref( "helpers.private_mime_types_file", buf1, &size );
    PREF_GetCharPref( "helpers.global_mime_types_file", buf2, &size );
    NET_InitFileFormatTypes( buf1, buf2 );
//#warning Work with changed files here. How do you do that? Kalle.

    PREF_GetCharPref( "helpers.private_mailcap_file", buf1, &size );
    PREF_GetCharPref( "helpers.global_mailcap_file", buf2, &size );
    NET_RegisterConverters( buf1, buf2 );
//#warning Work with changed files here. How do you do that? Kalle.

#ifndef NO_WEB_FONTS
    /* Register webfont converters */
    NF_RegisterConverters();
#endif /* NO_WEB_FONTS */

#if 0 // NOT YET
    /* Plugins go on top of all this */
    fe_RegisterPluginConverters();
#endif
}
