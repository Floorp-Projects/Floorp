/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *  Copyright (c) 1995 Regents of the University of Michigan.
 *  All rights reserved.
 */
/*
 *  os-ip.c -- platform-specific TCP & UDP related code
 */

#if 0
#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1995 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif
#endif

#include "ldap-int.h"
#ifdef LDAP_CONNECT_MUST_NOT_BE_INTERRUPTED
#include <signal.h>
#endif

#ifdef NSLDAPI_HAVE_POLL
#include <poll.h>
#endif

#ifdef _WINDOWS
#define CLOSESOCKET(_s) closesocket((_s))
#else
#define CLOSESOCKET(_s) close((_s))
#endif

struct selectinfo {
	fd_set		si_readfds;
	fd_set		si_writefds;
	fd_set		si_use_readfds;
	fd_set		si_use_writefds;
#ifdef NSLDAPI_HAVE_POLL
	struct pollfd	*si_pollfds;
	int		si_pollfds_size;
#endif
};

#ifdef NSLDAPI_HAVE_POLL
static int add_to_pollfds( int fd, struct selectinfo *sip, short events );
static int clear_from_pollfds( int fd, struct selectinfo *sip,
    short events );
static int find_in_pollfds( int fd, struct selectinfo *sip, short revents );
#endif

#ifdef irix
/*
 * XXXmcs: on IRIX NSPR's poll() and select() wrappers will crash if NSPR
 * has not been initialized.  We work around the problem by bypassing
 * the NSPR wrapper functions and going directly to the OS' functions.
 */
#define NSLDAPI_POLL		_poll
#define NSLDAPI_SELECT		_select
extern int _poll(struct pollfd *fds, unsigned long nfds, int timeout);
extern int _select(int nfds, fd_set *readfds, fd_set *writefds,
        fd_set *exceptfds, struct timeval *timeout);
#else
#define NSLDAPI_POLL		poll
#define NSLDAPI_SELECT		select
#endif


int
nsldapi_connect_to_host( LDAP *ld, Sockbuf *sb, char *host,
	unsigned long address, int port, int async, int secure )
/*
 * if host == NULL, connect using address
 * "address" and "port" must be in network byte order
 * zero is returned upon success, -1 if fatal error, -2 EINPROGRESS
 * if -1 is returned, ld_errno is set
 * non-zero async means don't wait for connect
 */
{
	int			rc, i, s, connected, use_hp;
	struct sockaddr_in	sin;
	char			**addrlist, *ldhpbuf, *ldhpbuf_allocd;
	LDAPHostEnt		ldhent, *ldhp;
	struct hostent		*hp;
#ifdef GETHOSTBYNAME_BUF_T
	GETHOSTBYNAME_BUF_T	hbuf;
	struct hostent		hent;
#endif
	int			err;
#ifdef LDAP_ASYNC_IO
	int			status;	/* for ioctl call */
#endif

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_connect_to_host: %s:%d\n",
	    ( host == NULL ) ? "(by address)" : host,
	    ntohs( (unsigned short)port ), 0 );

	if ( secure && ld->ld_ssl_enable_fn == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
		return( -1 );
	}

	ldhpbuf_allocd = NULL;
	ldhp = NULL;
	hp = NULL;
	s = 0;
	connected = use_hp = 0;
	addrlist = NULL;

	if ( host != NULL && ( address = inet_addr( host )) == -1 ) {
		if ( ld->ld_dns_gethostbyname_fn == NULL ) {
			if (( hp = GETHOSTBYNAME( host, &hent, hbuf,
			    sizeof(hbuf), &err )) != NULL ) {
				addrlist = hp->h_addr_list;
			}
		} else {
			/*
			 * DNS callback installed... use it.
			 */
#ifdef GETHOSTBYNAME_buf_t
			/* avoid allocation by using hbuf if large enough */
			if ( sizeof( hbuf ) < ld->ld_dns_bufsize ) {
				ldhpbuf = ldhpbuf_allocd
				    = NSLDAPI_MALLOC( ld->ld_dns_bufsize );
			} else {
				ldhpbuf = (char *)hbuf;
			}
#else /* GETHOSTBYNAME_buf_t */
			ldhpbuf = ldhpbuf_allocd = NSLDAPI_MALLOC(
			    ld->ld_dns_bufsize );
#endif /* GETHOSTBYNAME_buf_t */

			if ( ldhpbuf == NULL ) {
				LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL,
				    NULL );
				return( -1 );
			}

			if (( ldhp = ld->ld_dns_gethostbyname_fn( host,
			    &ldhent, ldhpbuf, ld->ld_dns_bufsize, &err,
			    ld->ld_dns_extradata )) != NULL ) {
				addrlist = ldhp->ldaphe_addr_list;
			}
		}

		if ( addrlist == NULL ) {
			LDAP_SET_LDERRNO( ld, LDAP_CONNECT_ERROR, NULL, NULL );
			LDAP_SET_ERRNO( ld, EHOSTUNREACH );  /* close enough */
			if ( ldhpbuf_allocd != NULL ) {
				NSLDAPI_FREE( ldhpbuf_allocd );
			}
			return( -1 );
		}
		use_hp = 1;
	}

	rc = -1;
	for ( i = 0; !use_hp || ( addrlist[ i ] != 0 ); i++ ) {
		if ( ld->ld_socket_fn == NULL ) {
			s = socket( AF_INET, SOCK_STREAM, 0 );
		} else {
			s = ld->ld_socket_fn( AF_INET, SOCK_STREAM, 0 );
		}

		/*
		 * if the socket() call failed or it returned a socket larger
		 * than we can deal with, return a "local error."
		 */
#ifdef _WINDOWS
		if ( s < 0 ) {
#elif NSLDAPI_HAVE_POLL
		if ( s < 0 || ( ld->ld_select_fn != NULL && s >= FD_SETSIZE )) {
#else	/* not on Windows and do not have poll() */
		if ( s < 0 || s >= FD_SETSIZE ) {
#endif
			char		*errmsg;

			if ( s < 0 ) {
				errmsg = "unable to create a socket";
			} else {
				errmsg = "can't use socket >= FD_SETSIZE";
				if ( ld->ld_close_fn == NULL ) {
					CLOSESOCKET( s );
				} else {
					ld->ld_close_fn( s );
				}
			}
			errmsg = nsldapi_strdup( errmsg );
			LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, errmsg );
			if ( ldhpbuf_allocd != NULL ) {
				NSLDAPI_FREE( ldhpbuf_allocd );
			}
			return( -1 );
		}

#ifdef LDAP_ASYNC_IO
		status = 1;
		if ( async ) {
			if ( ld->ld_ioctl_fn == NULL ) {
				err = ioctl( s, FIONBIO, (caddr_t)&status );
			} else {
				err = ld->ld_ioctl_fn( s, FIONBIO,
				    (caddr_t)&status );
			}
			if ( err == -1 ) {
				LDAPDebug( LDAP_DEBUG_ANY,
				    "FIONBIO ioctl failed on %d\n", s, 0, 0 );
			}
		}
#endif /* LDAP_ASYNC_IO */

		(void)memset( (char *)&sin, 0, sizeof( struct sockaddr_in ));
		sin.sin_family = AF_INET;
		sin.sin_port = port;

		if ( secure && ld->ld_ssl_enable_fn( s ) < 0 ) {
			if ( ld->ld_close_fn == NULL ) {
				CLOSESOCKET( s );
			} else {
				ld->ld_close_fn( s );
			}
			LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
			if ( ldhpbuf_allocd != NULL ) {
				NSLDAPI_FREE( ldhpbuf_allocd );
			}
			return( -1 );
		}

		SAFEMEMCPY( (char *) &sin.sin_addr.s_addr,
		    ( use_hp ? (char *) addrlist[ i ] :
		    (char *) &address ), sizeof( sin.sin_addr.s_addr) );

		if ( ld->ld_connect_fn == NULL ) {
#ifdef LDAP_CONNECT_MUST_NOT_BE_INTERRUPTED
/*
 * Block all of the signals that might interrupt connect() since there
 * is an OS bug that causes connect() to fail if it is restarted.  Look in
 * ns/netsite/ldap/include/portable.h for the definition of
 * LDAP_CONNECT_MUST_NOT_BE_INTERRUPTED
 */
			sigset_t	ints_off, oldset;

			sigemptyset( &ints_off );
			sigaddset( &ints_off, SIGALRM );
			sigaddset( &ints_off, SIGIO );
			sigaddset( &ints_off, SIGCLD );

			sigprocmask( SIG_BLOCK, &ints_off, &oldset );
#endif
			err = connect( s, (struct sockaddr *)&sin,
				sizeof( struct sockaddr_in ));
#ifdef LDAP_CONNECT_MUST_NOT_BE_INTERRUPTED
/*
 * restore original signal mask
 */
			sigprocmask( SIG_SETMASK, &oldset, 0 );
#endif
		} else {
		        if (ld->ld_options & LDAP_BITOPT_ASYNC)
			{
			    err = 0;
			}
			else {
			    err = ld->ld_connect_fn( s,
				(struct sockaddr *)&sin,
				sizeof( struct sockaddr_in ));
			}
			
		}
		if ( err >= 0 ) {
			connected = 1;
			rc = 0;
			break;
		} else {
#ifdef LDAP_ASYNC_IO
			err = LDAP_GET_ERRNO( ld );
			if ( NSLDAPI_ERRNO_IO_INPROGRESS( err )) {
				LDAPDebug( LDAP_DEBUG_TRACE,
					"connect would block...\n", 0, 0, 0 );
				rc = -2;
				break;
			}
#endif /* LDAP_ASYNC_IO */

#ifdef LDAP_DEBUG
			if ( ldap_debug & LDAP_DEBUG_TRACE ) {
				perror( (char *)inet_ntoa( sin.sin_addr ));
			}
#endif
			if ( ld->ld_close_fn == NULL ) {
				CLOSESOCKET( s );
			    
			} else {
			    ld->ld_close_fn( s );
			}
			if ( !use_hp ) {
				break;
			}
		}
	}

	if ( ldhpbuf_allocd != NULL ) {
		NSLDAPI_FREE( ldhpbuf_allocd );
	}

	sb->sb_sd = s;

	if ( connected ) {
		LDAPDebug( LDAP_DEBUG_TRACE, "sd %d connected to: %s\n",
		    s, inet_ntoa( sin.sin_addr ), 0 );
	}

	if ( rc == -1 ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONNECT_ERROR, NULL, NULL );
	}

	return( rc );
}


void
nsldapi_close_connection( LDAP *ld, Sockbuf *sb )
{
    if ( ld->ld_close_fn == NULL ) {
	    CLOSESOCKET( sb->sb_sd );
    } else {
	    ld->ld_close_fn( sb->sb_sd );
	
    }
}


#ifdef KERBEROS
char *
nsldapi_host_connected_to( Sockbuf *sb )
{
	struct hostent		*hp;
	char			*p;
	int			len;
	struct sockaddr_in	sin;

	(void)memset( (char *)&sin, 0, sizeof( struct sockaddr_in ));
	len = sizeof( sin );
	if ( getpeername( sb->sb_sd, (struct sockaddr *)&sin, &len ) == -1 ) {
		return( NULL );
	}

	/*
	 * do a reverse lookup on the addr to get the official hostname.
	 * this is necessary for kerberos to work right, since the official
	 * hostname is used as the kerberos instance.
	 */
/* XXXmcs: need to use DNS callbacks here XXX */
XXX
	if (( hp = gethostbyaddr( (char *) &sin.sin_addr,
	    sizeof( sin.sin_addr ), AF_INET )) != NULL ) {
		if ( hp->h_name != NULL ) {
			return( nsldapi_strdup( hp->h_name ));
		}
	}

	return( NULL );
}
#endif /* KERBEROS */


void
nsldapi_mark_select_write( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	LDAP_MUTEX_LOCK( ld, LDAP_SELECT_LOCK );
	sip = (struct selectinfo *)ld->ld_selectinfo;

#ifdef NSLDAPI_HAVE_POLL
	if ( ld->ld_select_fn == NULL ) {
		if ( add_to_pollfds( sb->sb_sd, sip, POLLOUT )) {
			++ld->ld_selectwritecnt;
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
		return;
	}
#endif

	if ( !FD_ISSET( sb->sb_sd, &sip->si_writefds )) {
		FD_SET( sb->sb_sd, &sip->si_writefds );
		++ld->ld_selectwritecnt;
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
}


void
nsldapi_mark_select_read( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	LDAP_MUTEX_LOCK( ld, LDAP_SELECT_LOCK );
	sip = (struct selectinfo *)ld->ld_selectinfo;

#ifdef NSLDAPI_HAVE_POLL
	if ( ld->ld_select_fn == NULL ) {
		if ( add_to_pollfds( sb->sb_sd, sip, POLLIN )) {
			++ld->ld_selectreadcnt;
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
		return;
	}
#endif

	if ( !FD_ISSET( sb->sb_sd, &sip->si_readfds )) {
		FD_SET( sb->sb_sd, &sip->si_readfds );
		++ld->ld_selectreadcnt;
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
}


void
nsldapi_mark_select_clear( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	LDAP_MUTEX_LOCK( ld, LDAP_SELECT_LOCK );
	sip = (struct selectinfo *)ld->ld_selectinfo;

#ifdef NSLDAPI_HAVE_POLL
	if ( ld->ld_select_fn == NULL ) {
		if ( clear_from_pollfds( sb->sb_sd, sip, POLLIN )) {
			--ld->ld_selectreadcnt;
		}
		if ( clear_from_pollfds( sb->sb_sd, sip, POLLOUT )) {
			--ld->ld_selectwritecnt;
		}
		LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
		return;
	}
#endif

	if ( FD_ISSET( sb->sb_sd, &sip->si_writefds )) {
		FD_CLR( sb->sb_sd, &sip->si_writefds );
		--ld->ld_selectwritecnt;
	}
	if ( FD_ISSET( sb->sb_sd, &sip->si_readfds )) {
		FD_CLR( sb->sb_sd, &sip->si_readfds );
		--ld->ld_selectreadcnt;
	}
	LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
}


int
nsldapi_is_write_ready( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	LDAP_MUTEX_LOCK( ld, LDAP_SELECT_LOCK );
	sip = (struct selectinfo *)ld->ld_selectinfo;

#ifdef NSLDAPI_HAVE_POLL
	if ( ld->ld_select_fn == NULL ) {
		LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
		/*
		 * if we are using poll() we do something a little tricky: if
		 * any bits in the socket's returned events field other than
		 * POLLIN (ready for read) are set, we return true.  This
		 * is done so we notice when a server closes a connection
		 * or when another error occurs.  The actual error will be
		 * noticed later when we call write() or send().
		 */
		return( find_in_pollfds( sb->sb_sd, sip, ~POLLIN ));
	}
#endif

	LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
	return( FD_ISSET( sb->sb_sd, &sip->si_use_writefds ));
}


int
nsldapi_is_read_ready( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;

	LDAP_MUTEX_LOCK( ld, LDAP_SELECT_LOCK );
	sip = (struct selectinfo *)ld->ld_selectinfo;

#ifdef NSLDAPI_HAVE_POLL
	if ( ld->ld_select_fn == NULL ) {
		LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
		/*
		 * if we are using poll() we do something a little tricky: if
		 * any bits in the socket's returned events field other than
		 * POLLOUT (ready for write) are set, we return true.  This
		 * is done so we notice when a server closes a connection
		 * or when another error occurs.  The actual error will be
		 * noticed later when we call read() or recv().
		 */
		return( find_in_pollfds( sb->sb_sd, sip, ~POLLOUT ));
	}
#endif

	LDAP_MUTEX_UNLOCK( ld, LDAP_SELECT_LOCK );
	return( FD_ISSET( sb->sb_sd, &sip->si_use_readfds ));
}


void *
nsldapi_new_select_info()
{
	struct selectinfo	*sip;

	if (( sip = (struct selectinfo *)NSLDAPI_CALLOC( 1,
	    sizeof( struct selectinfo ))) != NULL ) {
		FD_ZERO( &sip->si_readfds );
		FD_ZERO( &sip->si_writefds );
	}

	return( (void *)sip );
}


void
nsldapi_free_select_info( void *vsip )
{
	struct selectinfo	*sip = (struct selectinfo *)vsip;

#ifdef NSLDAPI_HAVE_POLL
	if ( sip->si_pollfds != NULL ) {
		NSLDAPI_FREE( sip->si_pollfds );
	}
#endif
	NSLDAPI_FREE( sip );
}


int
nsldapi_do_ldap_select( LDAP *ld, struct timeval *timeout )
{
	struct selectinfo	*sip;
	static int		tblsize = 0;

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_do_ldap_select\n", 0, 0, 0 );

	if ( tblsize == 0 ) {
#if defined(_WINDOWS) || defined(XP_OS2)
		tblsize = FOPEN_MAX; /* ANSI spec. */
#else
#ifdef USE_SYSCONF
		tblsize = sysconf( _SC_OPEN_MAX );
#else /* USE_SYSCONF */
		tblsize = getdtablesize();
#endif /* USE_SYSCONF */
#endif /* _WINDOWS */

		if ( tblsize >= FD_SETSIZE ) {
			/*
			 * clamp value so we don't overrun the fd_set structure
			 */
			tblsize = FD_SETSIZE - 1;
		}
	}

	if ( ld->ld_selectreadcnt <= 0 && ld->ld_selectwritecnt <= 0 ) {
		return( 0 );	/* simulate a timeout */
	}

	sip = (struct selectinfo *)ld->ld_selectinfo;

#ifdef NSLDAPI_HAVE_POLL
	if ( ld->ld_select_fn == NULL ) {
		int		to;

		if ( timeout == NULL ) {
			to = -1;
		} else {
			to = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
		}
		return( NSLDAPI_POLL( sip->si_pollfds, sip->si_pollfds_size,
		    to ));
	}
#endif

	sip->si_use_readfds = sip->si_readfds;
	sip->si_use_writefds = sip->si_writefds;

	if ( ld->ld_select_fn != NULL ) {
		return( ld->ld_select_fn( tblsize, &sip->si_use_readfds,
		    &sip->si_use_writefds, NULL, timeout ));
	} else {
#ifdef HPUX9
		return( NSLDAPI_SELECT( tblsize, (int *)&sip->si_use_readfds,
		    (int *)&sip->si_use_writefds, NULL, timeout ));
#else
		return( NSLDAPI_SELECT( tblsize, &sip->si_use_readfds,
		    &sip->si_use_writefds, NULL, timeout ));
#endif
	}
}

#ifdef NSLDAPI_HAVE_POLL

/*
 * returns 1 if "fd" was added to pollfds.
 * returns 1 if some of the bits in "events" were added to pollfds.
 * returns 0 if no changes were made.
 */
static int
add_to_pollfds( int fd, struct selectinfo *sip, short events )
{
	int	i, openslot;

	/* first we check to see if "fd" is already in our pollfds */
	openslot = -1;
	for ( i = 0; i < sip->si_pollfds_size; ++i ) {
		if ( sip->si_pollfds[ i ].fd == fd ) {
			if (( sip->si_pollfds[ i ].events & events )
			    != events ) {
				sip->si_pollfds[ i ].events |= events;
				return( 1 );
			} else {
				return( 0 );
			}
		}
		if ( sip->si_pollfds[ i ].fd == -1 && openslot == -1 ) {
			openslot = i;	/* remember for later */
		}
	}

	/*
	 * "fd" is not currently being poll'd on -- add to array.
	 * if we need to expand the pollfds array, we do it in increments of 5.
	 */
	if ( openslot == -1 ) {
		struct pollfd	*newpollfds;

		if ( sip->si_pollfds_size == 0 ) {
			newpollfds = (struct pollfd *)NSLDAPI_MALLOC(
			    5 * sizeof( struct pollfd ));
		} else {
			newpollfds = (struct pollfd *)NSLDAPI_REALLOC(
			    sip->si_pollfds, (5 + sip->si_pollfds_size) *
			    sizeof( struct pollfd ));
		}
		if ( newpollfds == NULL ) { /* XXXmcs: no way to return err! */
			return( 0 );
		}
		sip->si_pollfds = newpollfds;
		openslot = sip->si_pollfds_size;
		sip->si_pollfds_size += 5;
		for ( i = openslot + 1; i < sip->si_pollfds_size; ++i ) {
			sip->si_pollfds[ i ].fd = -1;
			sip->si_pollfds[ i ].events =
			    sip->si_pollfds[ i ].revents = 0;
		}
	}
	sip->si_pollfds[ openslot ].fd = fd;
	sip->si_pollfds[ openslot ].events = events;
	sip->si_pollfds[ openslot ].revents = 0;
	return( 1 );
}


/*
 * returns 1 if any "events" from "fd" were removed from pollfds
 * returns 0 of "fd" wasn't in pollfds or if events did not overlap.
 */
static int
clear_from_pollfds( int fd, struct selectinfo *sip,
    short events )
{
	int	i;

	for ( i = 0; i < sip->si_pollfds_size; ++i ) {
		if ( sip->si_pollfds[i].fd == fd ) {
			if (( sip->si_pollfds[ i ].events & events ) != 0 ) {
				sip->si_pollfds[ i ].events &= ~events;
				if ( sip->si_pollfds[ i ].events == 0 ) {
					sip->si_pollfds[i].fd = -1;
				}
				return( 1 );	/* events overlap */
			} else {
				return( 0 );	/* events do not overlap */
			}
		}
	}

	return( 0 );	/* "fd" was not found */
}


/*
 * returns 1 if any "revents" from "fd" were set in pollfds revents field.
 * returns 0 if not.
 */
static int
find_in_pollfds( int fd, struct selectinfo *sip, short revents )
{
	int	i;

	for ( i = 0; i < sip->si_pollfds_size; ++i ) {
		if ( sip->si_pollfds[i].fd == fd ) {
			if (( sip->si_pollfds[ i ].revents & revents ) != 0 ) {
				return( 1 );	/* revents overlap */
			} else {
				return( 0 );	/* revents do not overlap */
			}
		}
	}

	return( 0 );	/* "fd" was not found */
}
#endif /* NSLDAPI_HAVE_POLL */


/******************************************************
 *  ntstubs.c - Stubs needed on NT when linking in
 *  the SSL code. If these stubs were not here, the 
 *  named functions below would not be located at link
 *  time, because there is no implementation of the 
 *  functions for Win32 in cross-platform libraries.
 *
 ******************************************************/
#if defined( _WIN32 ) && defined ( NET_SSL )
/*#include <xp_file.h>*/
typedef enum {xpAddrBook} XP_FileType ;
typedef FILE          * XP_File;
typedef char          * XP_FilePerm;
XP_File XP_FileOpen(const char* name, XP_FileType type, 
		    const XP_FilePerm permissions)
{
    return NULL;
}

char *
WH_FileName (const char *name, XP_FileType type)
{
	return NULL;
}
#endif /* WIN32 && NET_SSL */
