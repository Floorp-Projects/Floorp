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

#include "stdafx.h"

#include "prefapi.h"

#include "netsdoc.h"
#include "template.h"
#ifdef MOZ_LDAP
#include "dirprefs.h"
#endif
#ifdef MOZ_MAIL_NEWS
#include "compfrm.h"
#endif
#ifdef EDITOR
//#include "edview.h"
#endif

#ifdef NSPR20
#include "private/prpriv.h"
#endif

// This is the NSPR priority of the main GUI thread (ie. mozilla)

#ifndef NSPR20
#ifdef XP_WIN32
#define MOZILLA_THREAD_PRIORITY 15
#else
#define MOZILLA_THREAD_PRIORITY 2
#endif
#else
#ifdef XP_WIN32
#define MOZILLA_THREAD_PRIORITY PR_PRIORITY_NORMAL
#else
#define MOZILLA_THREAD_PRIORITY PR_PRIORITY_LOW
#endif
#endif

#if defined(OJI) || defined(JAVA) || defined(MOCHA)

#ifndef XP_PC
#define XP_PC
#endif
#ifdef XP_WIN32
#ifndef HW_THREADS
#define HW_THREADS
#endif
#else
#ifndef SW_THREADS
#define SW_THREADS
#endif
#endif

#ifndef X386
#define X386
#endif

#include "nspr.h"
#include "plevent.h"
#endif /* OJI || JAVA || MOCHA */

#if defined(OJI)
#include "jvmmgr.h"
#elif defined(JAVA)
#include "java.h"
#endif

#if defined(XP_PC) && !defined(_WIN32)
#if defined(JAVA) || defined(OJI)
extern "C" PR_PUBLIC_API(void) SuspendAllJavaThreads(void);
#endif
#endif

// Shared event queue used to pass messages between the java threads and
// the main navigator thread...

extern "C" {
PREventQueue *mozilla_event_queue;
PRThread     *mozilla_thread;
};


//
// Call layout with the red green and blue values of the current COLORREF
//
MODULE_PRIVATE void
wfe_SetLayoutColor(int type, COLORREF color)
{
#ifndef MOZ_NGLAYOUT
	uint8 red   = GetRValue(color);
	uint8 green	= GetGValue(color);
	uint8 blue  = GetBValue(color);

	LO_SetDefaultColor(type, red, green, blue);
#endif /* MOZ_NGLAYOUT */
}

void OpenDraftExit (URL_Struct *url_struct, int/*status*/,MWContext *pContext)
{
	XP_ASSERT (url_struct && pContext);

	if (!url_struct) return;

	NET_FreeURLStruct ( url_struct );
}

#ifdef MOZ_LDAP
// This function handles callbacks for all of the *bad* things which we
// are caching in netscape.h...In general, we should try to move these but
// the mnprefs.cpp & mnwizard.cpp are such a mess that I am punting for now
int PR_CALLBACK
DirServerListChanged(const char *pref, void *data)
{
	theApp.m_directories = DIR_GetDirServers();
	return TRUE;
}
#endif // MOZ_LDAP

// This function handles callbacks for all of the *bad* things which we
// are caching in netscape.h...In general, we should try to move these but
// the mnprefs.cpp & mnwizard.cpp are such a mess that I am punting for now
int PR_CALLBACK
WinFEPrefChangedFunc(const char *pref, void *data)
{
        if (!strcmpi(pref,"browser.startup.homepage")) {
		LPSTR	lpszHomePage = NULL;

                PREF_CopyCharPref("browser.startup.homepage", &lpszHomePage);
		theApp.m_pHomePage = lpszHomePage;
		if (lpszHomePage)
			XP_FREE(lpszHomePage);

        } else if (!strcmpi(pref,"browser.cache.directory")) {
		LPSTR	lpszCacheDir;

                PREF_CopyCharPref("browser.cache.directory",&lpszCacheDir);
		theApp.m_pCacheDir = lpszCacheDir;
		if (lpszCacheDir)
			XP_FREE(lpszCacheDir);
        } else if (!strcmpi(pref,"browser.bookmark_location")) {
		LPSTR	lpszPref;

                PREF_CopyCharPref("browser.bookmark_location",&lpszPref);
		theApp.m_pBookmarkFile = lpszPref;
		if (lpszPref)
			XP_FREE(lpszPref);
		}
		else if (!strcmpi(pref,"mime.types.all_defined")) { //Begin CRN_MIME
			fe_UpdateMControlMimeTypes();				
		}//End CRN_MIME

	return PREF_NOERROR;
};

#ifdef XP_WIN16
/////////////////////////////////////////////////////////////////////////////
// 16-bit RTL helper functions
//
// These functions are used by NSPR to access RTL functions that are
// not available from 16-bit DLLs...  They provide the necessary
// "mov DS, SS" entry prolog to fix "DS == SS" which is assumed
// by the MS RTL...
//
#ifdef DEBUG
extern "C" int _heapchk();
extern "C" {
	extern unsigned long hcCount;
	extern unsigned long hcMallocCount;
	extern unsigned long hcFreeCount;
	extern unsigned long hcLimit;
}

static void chk()
{
	static unsigned long checkCount;

	if (_heapchk() != _HEAPOK)
		__asm { int 3 }
	hcCount += 1;
	if (hcCount >= hcLimit)
		__asm { int 3 }
}

extern "C" static void aFchkstk(void)
{
}

#endif

#if defined(XP_WIN16)
//  Only used when debug build on win16.
BOOL PR_CALLBACK DefaultBlockingHook(void) {
     MSG msg;
    BOOL ret = FALSE;

    /*
     * Only dispatch messages if the current thread is mozilla...
     */
    if (mozilla_thread == PR_CurrentThread()) {
	/* get the next message if any */
	ret = (BOOL)PeekMessage(&msg,NULL,0,0,PM_NOREMOVE);
	/* if we got one, process it */
	if (ret) {
	    ret = theApp.NSPumpMessage();
	}
    }
     /* TRUE if we got a message */
     return ret;
}
#endif

#ifdef MOZ_USE_MS_MALLOC
/*--------------------------------------------------------------------------*/
/* 32-bit aligned memory management routines				    */
/*--------------------------------------------------------------------------*/
/*									    */
/* The strategy for aligning memory to 32-bit boundaries is to allocate an  */
/* extra 32-bits for each allocation - A 16-bit header and a 16-bit padding */
/*									    */
/* If the memory returned from	_fmalloc() is not 32-bit aligned, then the  */
/* pointer is adjusted forward by 2 bytes to correctly align the memory.    */
/* The resulting 2-byte header is initialized to PR_NON_ALIGNED_HEADER.     */
/* There is also an "extra" 2 byte padding at the end of the block which    */
/* are unused.								    */
/*									    */
/* If the memory returned from _fmalloc() is already 32-aligned, then the   */
/* pointer is adjusted forward by 4 bytes to maintain alignment.  The first */
/* 2 bytes are filled with a pad value in DEBUG mode and the next 2 bytes   */
/* are initialized to PR_ALIGNED_HEADER 				    */
/*									    */
/*--------------------------------------------------------------------------*/

#define PR_HEADER_SIZE		4
#define PR_ALIGNED_PAD		0xDEAD
#define PR_ALIGNED_HEADER	0xAD04
#define PR_NONALIGNED_HEADER	0xAD02

#define PR_IS_ALIGNED(p)	( !(OFFSETOF(p) & 0x03) )
#define PR_GET_HEADER_WORD(p)	( *((WORD*)(((char *)(p))-2)) )
#define PR_GET_HEADER_PAD(p)	( *((WORD*)(((char *)(p))-4)) )
#define PR_GET_ROOT(p)		( (void *)(((unsigned long)(p)) - (unsigned long)(PR_GET_HEADER_WORD(p) & 0x000F)) )

//
// This macro verifies that a memory block is 32-bit aligned and contains
// the proper header values...
//
#ifdef DEBUG
#define PR_VALIDATE_BLOCK(p)	ASSERT( PR_IS_ALIGNED(p) );					 \
				if (p) {							 \
				    if (PR_GET_HEADER_WORD(p) == PR_ALIGNED_HEADER) {		 \
					ASSERT( PR_GET_HEADER_PAD(p) == PR_ALIGNED_PAD );	 \
				    } else {							 \
					ASSERT( PR_GET_HEADER_WORD(p) == PR_NONALIGNED_HEADER ); \
				    }								 \
				}
#else
#define PR_VALIDATE_BLOCK(p)
#endif

void * malloc(size_t size) {
    char *p;
    /* allocate a 16-bit aligned block of memory */
    p = (char *)_fmalloc(size + PR_HEADER_SIZE);

    /* Memory is already 32-bit aligned */
    if ( PR_IS_ALIGNED(p) ) {
#ifdef DEBUG
	*((WORD *)p) = PR_ALIGNED_PAD;
#endif
	p  = ((char *)p) + 2;
	*((WORD *)p) = PR_ALIGNED_HEADER;

    }
    /* Align memory to 32-bit boundary */
    else {
	*((WORD *)p) = PR_NONALIGNED_HEADER;
    }
    p  = ((char *)p) + 2;

    PR_VALIDATE_BLOCK(p);
    return (void *)p;

}

void free(void *p) {
    void *h;

    if (p) {
	PR_VALIDATE_BLOCK(p);

	h = PR_GET_ROOT(p);
	ASSERT( (OFFSETOF(p) - OFFSETOF(h)) <= PR_HEADER_SIZE );
    } else {
	h = NULL;
    }
    _ffree(h);
}

void * realloc(void *p, size_t size) {
    size_t old_size;
    void *new_block;
    void *h;

    if (p) {
	PR_VALIDATE_BLOCK(p);

	h	 = PR_GET_ROOT(p);
	old_size = _fmsize(h) - (PR_GET_HEADER_WORD(p) == PR_ALIGNED_HEADER ? 4 : 2);
	ASSERT( (OFFSETOF(p) - OFFSETOF(h)) <= PR_HEADER_SIZE );
    } else {
	h	 = NULL;
	old_size = 0;
    }


    /* allocate a 32-bit aligned block of memory */
    new_block = malloc(size);

    if (p && new_block) {
	/* copy the original data into the new block of memory */
	memcpy(new_block, p, (old_size < size ? old_size : size));
	/* free the old block of memory */
	_ffree(h);
    }

    p = new_block;

    PR_VALIDATE_BLOCK(p);
    return p;
}

void * calloc(size_t size, size_t number) {
    void *p;
    long bytes;

    bytes = (long)size * (long)number;
    ASSERT(bytes < 0xFFF0L);

    /* allocate a 32-bit aligned block of memory */
    p = malloc((size_t)bytes);

    if (p) {
	memset(p, 0x00, (size_t)bytes);
    }
    PR_VALIDATE_BLOCK(p);
    return p;
}
#endif
/*--------------------------------------------------------------------------*/


void * PR_CALLBACK __ns_malloc(size_t size) {
    return malloc(size);
}

void PR_CALLBACK __ns_free(void *p) {
    free(p);
}

void* PR_CALLBACK __ns_realloc(void *p, size_t size) {
    return realloc(p, size);
}

void* PR_CALLBACK __ns_calloc(size_t size, size_t number) {
    return calloc(size, number);
}

int PR_CALLBACK __ns_gethostname(char * name, int namelen) {
    return gethostname(name, namelen);
}

struct hostent * PR_CALLBACK __ns_gethostbyname(const char * name) {
    return gethostbyname(name);
}

struct hostent * PR_CALLBACK __ns_gethostbyaddr(const char * addr, int len, int type) {
    return gethostbyaddr(addr, len, type);
}

char* PR_CALLBACK __ns_getenv(const char *varname)
{
    return getenv(varname);
}

int PR_CALLBACK __ns_putenv(const char *varname)
{
    return putenv(varname);
}

int PR_CALLBACK __ns_auxOutput(const char *string)
{
#ifdef DEBUG
    ::OutputDebugString(string);
#endif
    return 0;
}

void PR_CALLBACK __ns_exit(int exitCode)
{
    ASSERT(0);
    exit(exitCode);
}

size_t PR_CALLBACK __ns_strftime(char *s, size_t len, const char *fmt, const struct tm *p)
{
    return strftime(s, len, fmt, p);
}


u_long	PR_CALLBACK __ns_ntohl(u_long netlong)
{
    return ntohl(netlong);
}

u_short PR_CALLBACK __ns_ntohs(u_short netshort)
{
    return ntohs(netshort);
}

int PR_CALLBACK __ns_closesocket(SOCKET s)
{
    return closesocket(s);
}

int PR_CALLBACK __ns_setsockopt(SOCKET s, int level, int optname, const char FAR * optval, int optlen)
{
    return setsockopt(s, level, optname, optval, optlen);
}

SOCKET	PR_CALLBACK __ns_socket(int af, int type, int protocol)
{
    return socket(af, type, protocol);
}

int PR_CALLBACK __ns_getsockname(SOCKET s, struct sockaddr FAR *name, int FAR * namelen)
{
    return getsockname(s, name, namelen);
}

u_long	PR_CALLBACK __ns_htonl(u_long hostlong)
{
    return htonl(hostlong);
}

u_short PR_CALLBACK __ns_htons(u_short hostshort)
{
    return htons(hostshort);
}

unsigned long PR_CALLBACK __ns_inet_addr(const char FAR * cp)
{
    return inet_addr(cp);
}

int PR_CALLBACK __ns_WSAGetLastError(void)
{
    return WSAGetLastError();
}

int PR_CALLBACK __ns_connect(SOCKET s, const struct sockaddr FAR *name, int namelen)
{
    return connect(s, name, namelen);
}

int PR_CALLBACK __ns_recv(SOCKET s, char FAR * buf, int len, int flags)
{
    return recv(s, buf, len, flags);
}

int PR_CALLBACK __ns_ioctlsocket(SOCKET s, long cmd, u_long FAR *argp)
{
    return ioctlsocket(s, cmd, argp);
}

int PR_CALLBACK __ns_recvfrom(SOCKET s, char FAR * buf, int len, int flags, struct sockaddr FAR *from, int FAR * fromlen)
{
    return recvfrom(s, buf, len, flags, from, fromlen);
}

int PR_CALLBACK __ns_send(SOCKET s, const char FAR * buf, int len, int flags)
{
    return send(s, buf, len, flags);
}

int PR_CALLBACK __ns_sendto(SOCKET s, const char FAR * buf, int len, int flags, const struct sockaddr FAR *to, int tolen)
{
    return sendto(s, buf, len, flags, to, tolen);
}

SOCKET PR_CALLBACK __ns_accept(SOCKET s, struct sockaddr FAR *addr, int FAR *addrlen)
{
    return accept(s, addr, addrlen);
}

int PR_CALLBACK __ns_listen(SOCKET s, int backlog)
{
    return listen(s, backlog);
}

int PR_CALLBACK __ns_bind(SOCKET s, const struct sockaddr FAR *addr, int namelen)
{
    return bind(s, addr, namelen);
}

int PR_CALLBACK __ns_select(int nfds, fd_set FAR *readfds, fd_set FAR *writefds, fd_set FAR *exceptfds, const struct timeval FAR *timeout)
{
    return select(nfds, readfds, writefds, exceptfds, timeout);
}

int PR_CALLBACK __ns_getsockopt(SOCKET s, int level, int optname, char FAR * optval, int FAR *optlen)
{
    return getsockopt(s, level, optname, optval, optlen);
}

struct protoent * PR_CALLBACK __ns_getprotobyname(const char FAR * name)
{
    return getprotobyname(name);
}

int PR_CALLBACK __ns_WSAAsyncSelect(SOCKET s, HWND hWnd, u_int wMsg,
                                    long lEvent)
{
    return WSAAsyncSelect(s, hWnd, wMsg, lEvent);
}


void PR_CALLBACK __ns_WSASetLastError(int iError)
{
    WSASetLastError(iError);
}

int PR_CALLBACK MozillaNonblockingNetTicklerCallback(void)
{
//	We need to tickle the network and the UI if we
//	are stuck in java networking code.

    MSG msg;

    if (PR_CurrentThread() == mozilla_thread) {
        /* get the next message if any */
	if (PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)) {
	    (void) theApp.NSPumpMessage();
            return FALSE;
	}
    }
    return TRUE;
}

#endif /* XP_WIN16 */

void fe_InitJava()
{
}

void fe_InitNSPR(void* stackBase)
{
#ifdef XP_WIN16
#if !defined(NSPR20)
//
// The 16-bit MS RTL prohibits 16-bit DLLs from calling some RTL functions.
//   ie.
//	 malloc/free
//	 strftime
//	 sscanf
//	 etc...
// To fix this limitation, function pointers are passed to NSPR which
// route the RTL functions "into the EXE" where the actual RTL function
// is available.  (Oh what a tangled web we weave :-( )
//
static struct PRMethodCallbackStr DLLCallbacks = {
    __ns_malloc,
    __ns_realloc,
    __ns_calloc,
    __ns_free,
    __ns_gethostname,
    __ns_gethostbyname,
    __ns_gethostbyaddr,
    __ns_getenv,
    __ns_putenv,
    __ns_auxOutput,
    __ns_exit,
    __ns_strftime,

    __ns_ntohl,
    __ns_ntohs,
    __ns_closesocket,
    __ns_setsockopt,
    __ns_socket,
    __ns_getsockname,
    __ns_htonl,
    __ns_htons,
    __ns_inet_addr,
    __ns_WSAGetLastError,
    __ns_connect,
    __ns_recv,
    __ns_ioctlsocket,
    __ns_recvfrom,
    __ns_send,
    __ns_sendto,
    __ns_accept,
    __ns_listen,
    __ns_bind,
    __ns_select,
    __ns_getsockopt,
    __ns_getprotobyname,
    __ns_WSAAsyncSelect,
    __ns_WSASetLastError
    };

    // Perform the 16-bit ONLY callback initialization...
    PR_MDInit(&DLLCallbacks);
#endif	/* NSPR20 */
#endif /* XP_WIN16 */

#if defined(OJI) || defined(JAVA) || defined(MOCHA)
#ifndef NSPR20
    // Initialize the NSPR library
    PR_Init( "mozilla", MOZILLA_THREAD_PRIORITY, 1, stackBase);

    mozilla_thread = PR_CurrentThread();
#else
    PR_STDIO_INIT()
    mozilla_thread = PR_CurrentThread();
    PR_SetThreadGCAble();
    PR_SetThreadPriority(mozilla_thread, MOZILLA_THREAD_PRIORITY);
    PL_InitializeEventsLib("mozilla");
#endif
#endif /* OJI || JAVA || MOCHA */
}

BOOL fe_ShutdownJava()
{
	BOOL bRetval = TRUE;

#if defined(OJI)

    JVMMgr* jvmMgr = JVM_GetJVMMgr();
    if (jvmMgr == NULL) 
        return FALSE;
    bRetval = (jvmMgr->ShutdownJVM() == JVMStatus_Enabled);
    // XXX suspend all java threads
    jvmMgr->Release();

#elif defined(JAVA)

    bRetval = (LJ_ShutdownJava() == LJJavaStatus_Enabled);
#if defined(XP_PC) && !defined(_WIN32)
    SuspendAllJavaThreads();
#endif

#endif

	return(bRetval);
}

//
// Register our document icons
//
static const CString strMARKUP_KEY = "NetscapeMarkup";

BOOL fe_RegisterOLEIcon ( LPSTR szCLSIDObject,	 // Class ID of the Object
		       LPSTR szObjectName,    // Human Readable Object Name
		       LPSTR szIconPath )     // Path and Index of Icon
{
    HKEY hKey;
    LONG lRes = 0L;

    char lpszCLSID[]       = "CLSID";         // OLE Class ID Section of eg DB
    char lpszDefIcon[]     = "\\DefaultIcon"; // Default Icon Subkey
    char lpBuffer[64];			      // Working buffer

    // Open the reg db for the "CLSID" key, this top-level key
    // is where OLE2 and other OLE aware apps will look for
    // this information.

    lRes = RegOpenKey(HKEY_CLASSES_ROOT, lpszCLSID, &hKey);

    if (ERROR_SUCCESS != lRes)
      {
      TRACE("RegOpenKey failed.\n");
      return FALSE;
      }

    // Register the Object

    // Set the value of the CLSID Entry to the Human Readable
    // name of the Object

    lRes = RegSetValue( hKey,
			szCLSIDObject,
			REG_SZ,
			szObjectName,
			lstrlen(szObjectName)
		      );

    if (ERROR_SUCCESS != lRes)	// bail on failure
      {
      RegCloseKey(hKey);
      return FALSE;
      }

    // Build "defaulticon" subkey string  "{ <class id> }\\DefaultIcon"
    lstrcpy (lpBuffer, szCLSIDObject);
    lstrcat (lpBuffer, lpszDefIcon);

    // Set Object's default icon entry to point to the
    // default icon for the object

    lRes = RegSetValue( hKey,
			lpBuffer,
			REG_SZ,
			szIconPath,
			lstrlen(szIconPath)
		      );

    if (ERROR_SUCCESS != lRes)	// bail on failure
      {
      RegCloseKey(hKey);
      return FALSE;
      }

    // Close the reg db

    RegCloseKey(hKey);


//
//
//

    lRes = RegOpenKey(HKEY_CLASSES_ROOT, (const char*)strMARKUP_KEY, &hKey);

    if (ERROR_SUCCESS != lRes)
      {
      TRACE("RegOpenKey failed.\n");
      return FALSE;
      }

    // Set Object's default icon entry to point to the
    // default icon for the object

    lRes = RegSetValue( hKey,
			lpszCLSID,
			REG_SZ,
			szCLSIDObject,
			lstrlen(szCLSIDObject)
		      );

    if (ERROR_SUCCESS != lRes)	// bail on failure
      {
      RegCloseKey(hKey);
      return FALSE;
      }


    // Set Object's default icon entry to point to the
    // default icon for the object

    lRes = RegSetValue( hKey,
                        "DefaultIcon",
			REG_SZ,
			szIconPath,
			lstrlen(szIconPath)
		      );

    if (ERROR_SUCCESS != lRes)	// bail on failure
      {
      RegCloseKey(hKey);
      return FALSE;
      }

    // Close the reg db

    RegCloseKey(hKey);



    return TRUE;
}

#if defined(XP_WIN16)
/****************************************************************************/
/*									    */
/* BEWARE! BEWARE! BEWARE! BEWARE! BEWARE! BEWARE! BEWARE! BEWARE! BEWARE!  */
/*									    */
/* The following code is duplicated from ns\modules\applet\src\lj_embed.c   */
/* It is ONLY required for 16-bit until JAVA becomes integrated into the    */
/* build!								    */
/*									    */
/* BEWARE! BEWARE! BEWARE! BEWARE! BEWARE! BEWARE! BEWARE! BEWARE! BEWARE!  */
/*									    */
/****************************************************************************/
#if defined(MOCHA) && !defined(JAVA)
#include "prevent.h"
#include "prlog.h"

PR_LOG_DEFINE(Event);

/*
 * There is currently a race between which thread destroys synchronous
 *   events.  See bug 32832 for a full description.
 */
void
LJ_ProcessEvent()
{
    PREvent* event;

    for (;;) {
        PR_LOG_BEGIN(Event, debug, ("$$$ getting event\n"));

	event = PR_GetEvent(mozilla_event_queue);
	if (event == NULL) {
	    return;
	}

	if(event->synchronousResult) {
	    PR_HandleEvent(event);
            PR_LOG_END(Event, debug, ("$$$ done with sync event\n"));
	}
	else {
	    PR_HandleEvent(event);
            PR_LOG_END(Event, debug, ("$$$ done with async event\n"));
	    PR_DestroyEvent(event);
	}

    }
}
#endif	// MOCHA && !JAVA
#endif	// XP_WIN16

#ifdef XP_WIN16
#if defined(NSPR20)
#include "private\prpriv.h"
#endif
void fe_yield(void)
{
	PR_Sleep(PR_INTERVAL_NO_WAIT);
}
#endif
