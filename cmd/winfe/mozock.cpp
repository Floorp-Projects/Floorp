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

//	Purpose of this file is to implement an intermediate layer to our network
//		services, the winsock.
//	This intermediate layer will be able to function with and without a working
//		winsock being present.
//	The attempt to activate the winsock happens as would normally be expected,
//		through the calling application's entry point to us, WSAStartup.


//	Name of the winsock we would like to load.
//	Diffs between OSs, Win32s is out in the cold if running 32 bits unless
//		they also have a winsock name wsock32.dll.
#ifdef XP_WIN16
#define SZWINSOCK "winsock.dll"
#else
#define SZWINSOCK "wsock32.dll"
#endif

//	Here is the enumeration for the winsock functions we have currently
//		overridden (needed to run).  Add more when needed.
//	We use these to access proc addresses, and to hold a table of strings
//		to obtain the proc addresses.
enum SockProc	{
	sp_WSAAsyncGetHostByName = 0,
	sp_WSAAsyncSelect,
	sp_WSACleanup,
	sp_WSAGetLastError,
	sp_WSASetLastError,
	sp_WSAStartup,
	sp___WSAFDIsSet,
	sp_accept,
	sp_bind,
	sp_closesocket,
	sp_connect,
	sp_gethostbyname,
	sp_gethostbyaddr,
	sp_gethostname,
	sp_getpeername,
	sp_getsockname,
	sp_getsockopt,
        sp_getprotobyname,
	sp_htonl,
	sp_htons,
	sp_inet_addr,
	sp_ioctlsocket,
	sp_listen,
	sp_ntohl,
	sp_ntohs,
	sp_recv,
	sp_recvfrom,
	sp_select,
	sp_send,
	sp_sendto,
	sp_setsockopt,
	sp_shutdown,
	sp_socket,
	sp_inet_ntoa,

	sp_MaxProcs	//	Total count.
};

//	Array of function names used in GetProcAddress to fill in our
//		proc array when needed.
//	This array must match the enumerations exactly.
char *spName[(SockProc)sp_MaxProcs] =	{
	"WSAAsyncGetHostByName",
	"WSAAsyncSelect",
	"WSACleanup",
	"WSAGetLastError",
	"WSASetLastError",
	"WSAStartup",
	"__WSAFDIsSet",
	"accept",
	"bind",
	"closesocket",
	"connect",
	"gethostbyname",
	"gethostbyaddr",
	"gethostname",
	"getpeername",
	"getsockname",
	"getsockopt",
        "getprotobyname",
	"htonl",
	"htons",
	"inet_addr",
	"ioctlsocket",
	"listen",
	"ntohl",
	"ntohs",
	"recv",
        "recvfrom",
	"select",
	"send",
        "sendto",
	"setsockopt",
	"shutdown",
	"socket",
	"inet_ntoa"
};

//	Array of proc addresses to the winsock functions.
//	These can be NULL, indicating their absence (as in the case we couldn't
//		load the winsock.dll or one of the functions wasn't loaded).
//	The procs assigned in must corellate with the enumerations exactly.
FARPROC spArray[(SockProc)sp_MaxProcs];

//	Typedef all the different types of functions that we must cast the
//		procs to in order to call without the compiler barfing.
//	Prefix is always sp.
//	Retval is next, spelled out.
//	Parameters in their order are next, spelled out.
typedef int (PASCAL FAR *sp_int_WORD_LPWSADATA)(WORD, LPWSADATA);
typedef int (PASCAL FAR *sp_int_void)(void);
typedef HANDLE (PASCAL FAR *sp_HANDLE_HWND_uint_ccharFARp_charFARp_int)(HWND, unsigned int, const char FAR *, char FAR *, int);
typedef int (PASCAL FAR *sp_int_SOCKET_HWND_uint_long)(SOCKET, HWND, unsigned int, long);
typedef void (PASCAL FAR *sp_void_int)(int);
typedef int (PASCAL FAR *sp_int_SOCKET_fdsetFARp)(SOCKET, fd_set FAR *);
typedef SOCKET(PASCAL FAR *sp_SOCKET_SOCKET_sockaddrFARp_intFARp)(SOCKET, struct sockaddr FAR *, int FAR *);
typedef int (PASCAL FAR *sp_int_SOCKET_csockaddrFARp_int)(SOCKET, const struct sockaddr FAR *, int);
typedef int (PASCAL FAR *sp_int_SOCKET)(SOCKET);
typedef struct hostent FAR *(PASCAL FAR *sp_hostentFARp_ccharFARp)(const char FAR *);
typedef struct hostent FAR *(PASCAL FAR *sp_hostentFARp_ccharFARp_int_int)(const char FAR *, int, int);
typedef int (PASCAL FAR *sp_int_charFARp_int)(char FAR *, int);
typedef int (PASCAL FAR *sp_int_SOCKET_sockaddrFARp_intFARp)(SOCKET, struct sockaddr FAR *, int FAR *);
typedef int (PASCAL FAR *sp_int_SOCKET_int_int_charFARp_intFARp)(SOCKET, int, int, char FAR *, int FAR *);
typedef u_long (PASCAL FAR *sp_ulong_ulong)(u_long);
typedef u_short (PASCAL FAR *sp_ushort_ushort)(u_short);
typedef unsigned long (PASCAL FAR *sp_ulong_ccharFARp)(const char FAR *);
typedef int (PASCAL FAR *sp_int_SOCKET_long_ulongFARp)(SOCKET, long, u_long FAR *);
typedef int (PASCAL FAR *sp_int_SOCKET_int)(SOCKET, int);
typedef int (PASCAL FAR *sp_int_SOCKET_charFARp_int_int)(SOCKET, char FAR *, int, int);
typedef int (PASCAL FAR *sp_int_SOCKET_charFARp_int_int_sockaddrFARp_intFARp)(SOCKET, char FAR *, int, int, struct sockaddr FAR*, int FAR *);
typedef int (PASCAL FAR *sp_int_int_fdsetFARp_fdsetFARp_fdsetFARp_ctimevalFARp)(int, fd_set FAR *, fd_set FAR *, fd_set FAR *, const struct timeval FAR *);
typedef int (PASCAL FAR *sp_int_SOCKET_ccharFARp_int_int)(SOCKET, const char FAR *, int, int);
typedef int (PASCAL FAR *sp_int_SOCKET_ccharFARp_int_int_csockaddrFARp_int)(SOCKET, const char FAR *, int, int, const struct sockaddr FAR*, int);
typedef int (PASCAL FAR *sp_int_SOCKET_int_int_ccharFARp_int)(SOCKET, int, int, const char FAR *, int);
typedef SOCKET (PASCAL FAR *sp_SOCKET_int_int_int)(int, int, int);
typedef char FAR * (PASCAL FAR *sp_charFARp_in_addr)(struct in_addr in);
typedef struct protoent FAR * (PASCAL FAR *sp_protoentFARcchar)(const char FAR *);

//	Handle to the winsock, if loaded.
HINSTANCE hWinsock = NULL;

#ifdef XP_WIN16
//	Last error code for the winsock.
int ispError = 0;
#endif

//	Quick macro to tell if the winsock has actually loaded for a particular
//		function.
//  Debug version is a little more strict to make sure you get the names right.
#ifdef DEBUG
#define IsWinsockLoaded(sp)	(hWinsock != NULL && spArray[(SockProc)(sp)] != NULL)
#else   //  A little faster
#define IsWinsockLoaded(sp) (hWinsock != NULL)
#endif

//	Here are the functions that we have taken over by not directly linking
//		with the winsock import library or importing through the def file.
extern "C"	{

int PASCAL FAR WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData)	{
	//	Before doing anything, clear out our proc array.
	memset(spArray, 0, sizeof(spArray));

	//	Our default return value is failure, though we change this regardless.
	int iRetval = WSAVERNOTSUPPORTED;

	//	attempt to load the real winsock.
	hWinsock = LoadLibrary(SZWINSOCK);
#ifdef XP_WIN32
	if(hWinsock != NULL)	{
#else
	if(hWinsock > HINSTANCE_ERROR)	{
#endif
		//	Winsock was loaded.
		//	Get the proc addresses for each needed function next.
		int spTraverse;
		for(spTraverse = 0; spTraverse < (SockProc)sp_MaxProcs; spTraverse++)	{
			spArray[spTraverse] = GetProcAddress(hWinsock, spName[spTraverse]);
			ASSERT(spArray[spTraverse]);	//	Bad winsock?  Bad function name?
		}

		//	AllRight, attempt to make our first proxied call.
		if(IsWinsockLoaded(sp_WSAStartup))	{
			iRetval = ((sp_int_WORD_LPWSADATA)spArray[sp_WSAStartup])(wVersionRequested, lpWSAData);
		}

		//	If the return value is still an error at this point, we unload the DLL,
		//		so that we can act as though nothing happened and the user
		//		gets no network access.
		if(iRetval != 0)	{
			//	Clear out our proc array.
			memset(spArray, 0, sizeof(spArray));

			//	Free up the winsock.
			FreeLibrary(hWinsock);
			hWinsock = NULL;
		}
	}
#ifdef XP_WIN16
	else	{
		//	Failed to load.
		//	Set this to NULL so it is clear.
		hWinsock = NULL;
	}
#endif

	//	Check our return value, if it isn't success, then we need to fake
	//		our own winsock implementation.
	if(iRetval != 0)	{
		//	We always return success.
		iRetval = 0;

		//	Fill in the structure.
		//	Return the version requested as the version supported.
		lpWSAData->wVersion = wVersionRequested;
		lpWSAData->wHighVersion = wVersionRequested;

		//	Fill in a discription.
		strcpy(lpWSAData->szDescription, "Mozock DLL internal implementation.");
		strcpy(lpWSAData->szSystemStatus, "Winsock running, allowing no network access.");

		//	Report a nice round number for sockets and datagram sizes.
		lpWSAData->iMaxSockets = 4096;
		lpWSAData->iMaxUdpDg = 4096;

		//	No vendor information.
		lpWSAData->lpVendorInfo = NULL;
	}

	return(iRetval);
}

int PASCAL FAR WSACleanup(void)	{
	int iRetval = 0;

	//	Handling normally or internally.
	if(IsWinsockLoaded(sp_WSACleanup))	{
		//	Call their cleanup routine.
		//	We could set the return value here, but it is meaning less.
		//	We always return success.
		iRetval = ((sp_int_void)spArray[sp_WSACleanup])();
		ASSERT(iRetval == 0);
		iRetval = 0;
	}

	//	Wether or not it succeeded, we free off the library here.
	//	Clear out our proc table too.
	memset(spArray, 0, sizeof(spArray));
	if(hWinsock != NULL)	{
		FreeLibrary(hWinsock);
		hWinsock = NULL;
	}

	return(iRetval);
}

HANDLE PASCAL FAR WSAAsyncGetHostByName(HWND hWnd, unsigned int wMsg, const char FAR *name, char FAR *buf, int buflen)	{
	// Normal or shim.
	if(IsWinsockLoaded(sp_WSAAsyncGetHostByName))	{
		return(((sp_HANDLE_HWND_uint_ccharFARp_charFARp_int)spArray[sp_WSAAsyncGetHostByName])(hWnd, wMsg, name, buf, buflen));
	}

	//	Must return error here.
	//	Set our last error value to be that the net is down.
	WSASetLastError(WSAENETDOWN);
	return(NULL);
}

int PASCAL FAR WSAAsyncSelect(SOCKET s, HWND hWnd, unsigned int wMsg, long lEvent)	{
	//	Normal or shim.
	if(IsWinsockLoaded(sp_WSAAsyncSelect))	{
		return(((sp_int_SOCKET_HWND_uint_long)spArray[sp_WSAAsyncSelect])(s, hWnd, wMsg, lEvent));
	}

	//	Must return error here.
	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR WSAGetLastError(void)	{
	//	See if someone else can handle.
	if(IsWinsockLoaded(sp_WSAGetLastError))	{
		return(((sp_int_void)spArray[sp_WSAGetLastError])());
	}

#ifdef XP_WIN16
	//	Fake it.
	int iRetval = ispError;
	ispError = 0;
	return(iRetval);
#else
	//	Use default OS handler.
	return(GetLastError());
#endif
}

void PASCAL FAR WSASetLastError(int iError)	{
	//	See if someone else can handle.
	if(IsWinsockLoaded(sp_WSASetLastError))	{
		((sp_void_int)spArray[sp_WSASetLastError])(iError);
		return;
	}

#ifdef XP_WIN16
	//	Fake it.
	ispError = iError;
	return;
#else
	//	Use default OS handler.
	SetLastError(iError);
	return;
#endif
}

int PASCAL FAR __WSAFDIsSet(SOCKET fd, fd_set FAR *set)	{
	//	See if someone else will handle.
	if(IsWinsockLoaded(sp___WSAFDIsSet))	{
		return(((sp_int_SOCKET_fdsetFARp)spArray[sp___WSAFDIsSet])(fd, set));
	}

	//	Default implementation.
	int i = set->fd_count;
 	while (i--)	{
		if (set->fd_array[i] == fd)	{
	 		return 1;
		}
	}
 	return 0;
}

SOCKET PASCAL FAR accept(SOCKET s, struct sockaddr FAR *addr, int FAR *addrlen)	{
	//	Internally or shim
	if(IsWinsockLoaded(sp_accept))	{
		return(((sp_SOCKET_SOCKET_sockaddrFARp_intFARp)spArray[sp_accept])(s, addr, addrlen));
	}

	//	Fail.
	WSASetLastError(WSAENETDOWN);
	return(INVALID_SOCKET);
}

int PASCAL FAR bind(SOCKET s, const struct sockaddr FAR *name, int namelen)	{
	//	Internally or shim
	if(IsWinsockLoaded(sp_bind))	{
		return(((sp_int_SOCKET_csockaddrFARp_int)spArray[sp_bind])(s, name, namelen));
	}

	//	Fail.
	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR closesocket(SOCKET s)	{
	//	Internally or shim.
	if(IsWinsockLoaded(sp_closesocket))	{
		return(((sp_int_SOCKET)spArray[sp_closesocket])(s));
	}

	//	Error.
	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR connect(SOCKET s, const struct sockaddr FAR *name, int namelen)	{
	//	Internally or shim.
	if(IsWinsockLoaded(sp_connect))	{
		return(((sp_int_SOCKET_csockaddrFARp_int)spArray[sp_connect])(s, name, namelen));
	}

	//	Err.
	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

struct hostent FAR * PASCAL FAR gethostbyname(const char FAR *name)	{
	if(IsWinsockLoaded(sp_gethostbyname))	{
		return(((sp_hostentFARp_ccharFARp)spArray[sp_gethostbyname])(name));
	}

	WSASetLastError(WSAENETDOWN);
	return(NULL);
}

struct hostent FAR * PASCAL FAR gethostbyaddr(const char FAR *addr, int len, int type)	{
	if(IsWinsockLoaded(sp_gethostbyaddr))	{
		return(((sp_hostentFARp_ccharFARp_int_int)spArray[sp_gethostbyaddr])(addr, len, type));
	}

	WSASetLastError(WSAENETDOWN);
	return(NULL);
}

int PASCAL FAR gethostname(char FAR *name, int namelen)	{
	if(IsWinsockLoaded(sp_gethostname))	{
		return(((sp_int_charFARp_int)spArray[sp_gethostname])(name, namelen));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR getpeername(SOCKET s, struct sockaddr FAR *name, int FAR *namelen)	{
	if(IsWinsockLoaded(sp_getpeername))	{
		return(((sp_int_SOCKET_sockaddrFARp_intFARp)spArray[sp_getpeername])(s, name, namelen));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR getsockname(SOCKET s, struct sockaddr FAR *name, int FAR *namelen)	{
	if(IsWinsockLoaded(sp_getsockname))	{
		return(((sp_int_SOCKET_sockaddrFARp_intFARp)spArray[sp_getsockname])(s, name, namelen));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR getsockopt(SOCKET s, int level, int optname, char FAR *optval, int FAR *optlen)	{
	if(IsWinsockLoaded(sp_getsockopt))	{
		return(((sp_int_SOCKET_int_int_charFARp_intFARp)spArray[sp_getsockopt])(s, level, optname, optval, optlen));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

struct protoent FAR * PASCAL getprotobyname(const char FAR * name) {
	if(IsWinsockLoaded(sp_getprotobyname))	{
		return(((sp_protoentFARcchar)spArray[sp_getprotobyname])(name));
	}

	WSASetLastError(WSAENETDOWN);
	return NULL;
}

u_long PASCAL FAR htonl(u_long hostlong)	{
	if(IsWinsockLoaded(sp_htonl))	{
		return(((sp_ulong_ulong)spArray[sp_htonl])(hostlong));
	}

#ifdef XP_WIN16
	return
	    (((hostlong&0xff)<<24) + ((hostlong&0xff00)<<8) +
	     ((hostlong&0xff0000)>>8) + ((hostlong&0xff000000)>>24));

#else
	//	Just return what was passed in.
	return(hostlong);
#endif
}

u_short PASCAL FAR htons(u_short hostshort)	{
	if(IsWinsockLoaded(sp_htons))	{
		return(((sp_ushort_ushort)spArray[sp_htons])(hostshort));
	}

#ifdef XP_WIN16
	return (((hostshort&0xff)<<8) + ((hostshort&0xff00)>>8));

#else
	//	Just return what was passed in.
	return(hostshort);
#endif
}

u_long PASCAL FAR ntohl(u_long hostlong)	{
	if(IsWinsockLoaded(sp_ntohl))	{
		return(((sp_ulong_ulong)spArray[sp_ntohl])(hostlong));
	}

#ifdef XP_WIN16
	return
	    (((hostlong&0xff)<<24) + ((hostlong&0xff00)<<8) +
	     ((hostlong&0xff0000)>>8) + ((hostlong&0xff000000)>>24));

#else
	//	Just return what was passed in.
	return(hostlong);
#endif
}

u_short PASCAL FAR ntohs(u_short hostshort)	{
	if(IsWinsockLoaded(sp_ntohs))	{
		return(((sp_ushort_ushort)spArray[sp_ntohs])(hostshort));
	}

#ifdef XP_WIN16
	return (((hostshort&0xff)<<8) + ((hostshort&0xff00)>>8));

#else
	//	Just return what was passed in.
	return(hostshort);
#endif
}

unsigned long PASCAL FAR inet_addr(const char FAR *cp)	{
	if(IsWinsockLoaded(sp_inet_addr))	{
		return(((sp_ulong_ccharFARp)spArray[sp_inet_addr])(cp));
	}

	return(INADDR_NONE);
}

int PASCAL FAR ioctlsocket(SOCKET s, long cmd, u_long FAR *argp)	{
	if(IsWinsockLoaded(sp_ioctlsocket))	{
		return(((sp_int_SOCKET_long_ulongFARp)spArray[sp_ioctlsocket])(s, cmd, argp));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR listen(SOCKET s, int backlog)	{
	if(IsWinsockLoaded(sp_listen))	{
		return(((sp_int_SOCKET_int)spArray[sp_listen])(s, backlog));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR recv(SOCKET s, char FAR *buf, int len, int flags)	{
	if(IsWinsockLoaded(sp_recv))	{
		return(((sp_int_SOCKET_charFARp_int_int)spArray[sp_recv])(s, buf, len, flags));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR recvfrom(SOCKET s, char FAR *buf, int len, int flags,
	    struct sockaddr FAR * from, int FAR * fromlen) {
	if(IsWinsockLoaded(sp_recvfrom))	{
	    return (((sp_int_SOCKET_charFARp_int_int_sockaddrFARp_intFARp)spArray[sp_recvfrom])(s, buf, len, flags, from, fromlen));
        }

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR select(int nfds, fd_set FAR *readfds, fd_set FAR *writefds, fd_set FAR *exceptfds, const struct timeval FAR *timeout)	{
    //  If there's nothing to do, stop now before we go off into dll land.
    //  Optimization, boyz.
    if((readfds && readfds->fd_count) || (writefds && writefds->fd_count) || (exceptfds && exceptfds->fd_count))    {
	    if(IsWinsockLoaded(sp_select))	{
		        return(((sp_int_int_fdsetFARp_fdsetFARp_fdsetFARp_ctimevalFARp)spArray[sp_select])(nfds, readfds, writefds, exceptfds, timeout));
	    }

	    WSASetLastError(WSAENETDOWN);
	    return(SOCKET_ERROR);
    }

    //  No need to go to the DLL, there is nothing to do.
    return(0);
}

int PASCAL FAR send(SOCKET s, const char FAR *buf, int len, int flags)	{
	if(IsWinsockLoaded(sp_send))	{
		return(((sp_int_SOCKET_ccharFARp_int_int)spArray[sp_send])(s, buf, len, flags));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR sendto(SOCKET s, const char FAR *buf, int len, int flags,
	    const struct sockaddr FAR * to, int tolen) {
	if(IsWinsockLoaded(sp_sendto))	{
	    return (((sp_int_SOCKET_ccharFARp_int_int_csockaddrFARp_int)spArray[sp_sendto])(s, buf, len, flags, to, tolen));
        }

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR setsockopt(SOCKET s, int level, int optname, const char FAR *optval, int optlen)	{
	if(IsWinsockLoaded(sp_setsockopt))	{
		return(((sp_int_SOCKET_int_int_ccharFARp_int)spArray[sp_setsockopt])(s, level, optname, optval, optlen));
	}	

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

int PASCAL FAR shutdown(SOCKET s, int how)	{
	if(IsWinsockLoaded(sp_shutdown))	{
		return(((sp_int_SOCKET_int)spArray[sp_shutdown])(s, how));
	}

	WSASetLastError(WSAENETDOWN);
	return(SOCKET_ERROR);
}

SOCKET PASCAL FAR socket(int af, int type, int protocol)	{
	if(IsWinsockLoaded(sp_socket))	{
		return(((sp_SOCKET_int_int_int)spArray[sp_socket])(af, type, protocol));
	}

	WSASetLastError(WSAENETDOWN);
	return(INVALID_SOCKET);
}

char FAR * PASCAL FAR inet_ntoa(struct in_addr in) {
	if(IsWinsockLoaded(sp_inet_ntoa))	{
		return ((sp_charFARp_in_addr)spArray[sp_inet_ntoa])(in);
	}

	WSASetLastError(WSAENETDOWN);
	return NULL;
}

};
