/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 * 
 * The contents of this file are subject to the Mozilla Public License Version 
 * 1.1 (the "License"); you may not use this file except in compliance with 
 * the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 * 
 * ***** END LICENSE BLOCK ***** */
/*
 * Microsoft Windows specifics for LIBLDAP DLL
 */
#include "ldap.h"
#include "lber.h"


#ifdef _WIN32
/* Lifted from Q125688
 * How to Port a 16-bit DLL to a Win32 DLL
 * on the MSVC 4.0 CD
 */
BOOL WINAPI DllMain (HANDLE hModule, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		 /* Code from LibMain inserted here.  Return TRUE to keep the
		    DLL loaded or return FALSE to fail loading the DLL.

		    You may have to modify the code in your original LibMain to
		    account for the fact that it may be called more than once.
		    You will get one DLL_PROCESS_ATTACH for each process that
		    loads the DLL. This is different from LibMain which gets
		    called only once when the DLL is loaded. The only time this
		    is critical is when you are using shared data sections.
		    If you are using shared data sections for statically
		    allocated data, you will need to be careful to initialize it
		    only once. Check your code carefully.

		    Certain one-time initializations may now need to be done for
		    each process that attaches. You may also not need code from
		    your original LibMain because the operating system may now
		    be doing it for you.
		 */
		/*
		 * 16 bit code calls UnlockData()
		 * which is mapped to UnlockSegment in windows.h
		 * in 32 bit world UnlockData is not defined anywhere
		 * UnlockSegment is mapped to GlobalUnfix in winbase.h
		 * and the docs for both UnlockSegment and GlobalUnfix say 
		 * ".. function is oboslete.  Segments have no meaning 
		 *  in the 32-bit environment".  So we do nothing here.
		 */
		/* If we are building a version that includes the security libraries,
		 * we have to initialize Winsock here. If not, we can defer until the
		 * first real socket call is made (in mozock.c).
		 */
#ifdef LINK_SSL
	{
		WSADATA wsaData;
		WSAStartup(0x0101, &wsaData);
	}
#endif

		break;

	case DLL_THREAD_ATTACH:
		/* Called each time a thread is created in a process that has
		   already loaded (attached to) this DLL. Does not get called
		   for each thread that exists in the process before it loaded
		   the DLL.

		   Do thread-specific initialization here.
		*/
		break;

	case DLL_THREAD_DETACH:
		/* Same as above, but called when a thread in the process
		   exits.

		   Do thread-specific cleanup here.
		*/
		break;

	case DLL_PROCESS_DETACH:
		/* Code from _WEP inserted here.  This code may (like the
		   LibMain) not be necessary.  Check to make certain that the
		   operating system is not doing it for you.
		*/
#ifdef LINK_SSL
		WSACleanup();
#endif

		break;
	}
	/* The return value is only used for DLL_PROCESS_ATTACH; all other
	conditions are ignored.  */
	return TRUE;   // successful DLL_PROCESS_ATTACH
}
#else
int CALLBACK
LibMain( HINSTANCE hinst, WORD wDataSeg, WORD cbHeapSize, LPSTR lpszCmdLine )
{
	/*UnlockData( 0 );*/
 	return( 1 );
}

BOOL CALLBACK __loadds WEP(BOOL fSystemExit)
{
	WSACleanup();
    return TRUE;
}

#endif

#ifdef LDAP_DEBUG
#ifndef _WIN32
#include <stdarg.h>
#include <stdio.h>

void LDAP_C LDAPDebug( int level, char* fmt, ... )
{
	static char debugBuf[1024];

	if (ldap_debug & level)
	{
		va_list ap;
		va_start (ap, fmt);
		_snprintf (debugBuf, sizeof(debugBuf), fmt, ap);
		va_end (ap);

		OutputDebugString (debugBuf);
	}
}
#endif
#endif

#ifndef _WIN32

/* The 16-bit version of the RTL does not implement perror() */

#include <stdio.h>

void perror( const char *msg )
{
	char buf[128];
	wsprintf( buf, "%s: error %d\n", msg, WSAGetLastError()) ;
	OutputDebugString( buf );
}

#endif
