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
 * macos.h: bridge unix and Mac for  LBER/LDAP
 */
#ifndef __LDAP_MACOS_H__
#define __LDAP_MACOS_H__
#if 0
		/*------------------*/
		#define ntohl( l )	(l)
		#define htonl( l )	(l)
		#define ntohs( s )	(s)
		#define htons( s )	(s)
		/*------------------*/
#endif

#define NSLDAPI_AVOID_OS_SOCKETS

#ifdef NO_GLOBALS

#ifdef macintosh	/* IUMagIDString declared in TextUtils.h under MPW */
#include <TextUtils.h>
#else /* macintosh */	/* IUMagIDString declared in Packages.h under ThinkC */
#include <Packages.h>
#endif /* macintosh */

#define strcasecmp( s1, s2 )	IUMagIDString( s1, s2, strlen( s1 ), \
					strlen( s2 ))
#else /* NO_GLOBALS */
int strcasecmp(  const char *s1,  const char *s2 );
int strncasecmp(  const char *s1,  const char *s2,  const long n );
#endif NO_GLOBALS

#include <Memory.h>	/* to get BlockMove() */

char *strdup( const char *s );

#ifndef isascii
#define isascii(c)	((unsigned)(c)<=0177)	/* for those who don't have this in ctype.h */
#endif isascii

/*
 * if we aren't supporting OpenTransport, we need to define some standard "errno" values here
 * the values should match those in OpenTransport.h
 */
#if !defined( SUPPORT_OPENTRANSPORT )
#define EHOSTUNREACH	65	/* No route to host */
#define EAGAIN		11	/* Resource temporarily unavailable */
#define EWOULDBLOCK	35	/* ditto */
#endif

int getopt(int nargc, char **nargv, char *ostr);

#include <time.h>
#include <stdlib.h>

#include "macsocket.h"
#include "tcp.h"
#if 0
		/*------------------*/
		#ifndef AF_INET
		/* these next few things are defined only for use by the LDAP socket I/O function callbacks */
		#define AF_INET			2
		#define SOCK_STREAM		1
		struct in_addr {
			unsigned long s_addr;
		};
		struct sockaddr_in {
			short			sin_family;
			unsigned short	sin_port;
			struct in_addr	sin_addr;
			char			sin_zero[8];
		};
		#endif
		/*------------------*/
#endif


#endif /* __LDAP_MACOS_H__ */
