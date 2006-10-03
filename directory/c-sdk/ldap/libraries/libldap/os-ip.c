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

/*
 * On platforms where poll() does not exist, we use select().
 * Therefore, we should increase the number of file descriptors
 * we can use by #defining FD_SETSIZE to a large number before
 * we include <sys/select.h> or its equivalent.  We do not need
 * to do this for Windows, because on that platform there is no
 * relationship between FD_SETSIZE and the highest numbered file
 * descriptor we can use.  See the document fdsetsize.txt in
 * this directory for a description of the setting of FD_SETSIZE
 * for the OS'es we care about.
 */
#ifndef NSLDAPI_HAVE_POLL
/* XXX value for BSDI? */
/* XXX value for macintosh (if applicable)? */
#endif

#include "ldap-int.h"
#ifdef NSLDAPI_CONNECT_MUST_NOT_BE_INTERRUPTED
#include <signal.h>
#endif

#ifdef NSLDAPI_HAVE_POLL
#include <poll.h>
#endif


#ifdef _WINDOWS
#define NSLDAPI_INVALID_OS_SOCKET( s )	((s) == INVALID_SOCKET)
#else
#define NSLDAPI_INVALID_OS_SOCKET( s )	((s) < 0 )	
#endif


#define NSLDAPI_POLL_ARRAY_GROWTH  5  /* grow arrays 5 elements at a time */


/*
 * Structures and union for tracking status of network sockets
 */
#ifdef NSLDAPI_HAVE_POLL
struct nsldapi_os_statusinfo {		/* used with native OS poll() */
	struct pollfd		*ossi_pollfds;
	int			ossi_pollfds_size;
};
#else /* NSLDAPI_HAVE_POLL */
struct nsldapi_os_statusinfo {		/* used with native OS select() */
	fd_set			ossi_readfds;
	fd_set			ossi_writefds;
	fd_set			ossi_use_readfds;
	fd_set			ossi_use_writefds;
};
#endif /* else NSLDAPI_HAVE_POLL */

struct nsldapi_cb_statusinfo {		/* used with ext. I/O poll() callback */
    LDAP_X_PollFD		*cbsi_pollfds;
    int				cbsi_pollfds_size;
};

/*
 * NSLDAPI_CB_POLL_MATCH() evaluates to non-zero (true) if the Sockbuf *sdp
 * matches the LDAP_X_PollFD pollfd.
 */
#ifdef _WINDOWS
#define NSLDAPI_CB_POLL_SD_CAST		(unsigned int)
#else
#define NSLDAPI_CB_POLL_SD_CAST
#endif
#define NSLDAPI_CB_POLL_MATCH( sbp, pollfd ) \
    ((sbp)->sb_sd == NSLDAPI_CB_POLL_SD_CAST ((pollfd).lpoll_fd) && \
    (sbp)->sb_ext_io_fns.lbextiofn_socket_arg == (pollfd).lpoll_socketarg)


struct nsldapi_iostatus_info {
	int				ios_type;
#define NSLDAPI_IOSTATUS_TYPE_OSNATIVE		1   /* poll() or select() */
#define NSLDAPI_IOSTATUS_TYPE_CALLBACK		2   /* poll()-like */
	int				ios_read_count;
	int				ios_write_count;
	union {
	    struct nsldapi_os_statusinfo	ios_osinfo;
	    struct nsldapi_cb_statusinfo	ios_cbinfo;
	} ios_status;
};

#ifndef NSLDAPI_AVOID_OS_SOCKETS
#ifdef NSLDAPI_HAVE_POLL
static int nsldapi_add_to_os_pollfds( int fd,
    struct nsldapi_os_statusinfo *pip, short events );
static int nsldapi_clear_from_os_pollfds( int fd,
    struct nsldapi_os_statusinfo *pip, short events );
static int nsldapi_find_in_os_pollfds( int fd,
    struct nsldapi_os_statusinfo *pip, short revents );
#endif /* NSLDAPI_HAVE_POLL */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */

static int nsldapi_iostatus_init_nolock( LDAP *ld );
static int nsldapi_add_to_cb_pollfds( Sockbuf *sb,
    struct nsldapi_cb_statusinfo *pip, short events );
static int nsldapi_clear_from_cb_pollfds( Sockbuf *sb,
    struct nsldapi_cb_statusinfo *pip, short events );
static int nsldapi_find_in_cb_pollfds( Sockbuf *sb,
    struct nsldapi_cb_statusinfo *pip, short revents );


#ifdef irix
#ifndef _PR_THREADS
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
#else /* _PR_THREADS */
#define NSLDAPI_POLL		poll
#define NSLDAPI_SELECT		select
#endif /* else _PR_THREADS */
#else /* irix */
#define NSLDAPI_POLL		poll
#define NSLDAPI_SELECT		select
#endif /* else irix */


static LBER_SOCKET nsldapi_os_socket( LDAP *ld, int secure, int domain,
	int type, int protocol );
static int nsldapi_os_ioctl( LBER_SOCKET s, int option, int *statusp );
static int nsldapi_os_connect_with_to( LBER_SOCKET s, struct sockaddr *name,
	int namelen, int msec_timeout );
#if defined(KERBEROS)
char * nsldapi_host_connected_to( LDAP *ld, Sockbuf *sb );
#endif

/*
 * Function typedefs used by nsldapi_try_each_host()
 */
typedef LBER_SOCKET (NSLDAPI_SOCKET_FN)( LDAP *ld, int secure, int domain,
	    int type, int protocol );
typedef int (NSLDAPI_IOCTL_FN)( LBER_SOCKET s, int option, int *statusp );
typedef int (NSLDAPI_CONNECT_WITH_TO_FN )( LBER_SOCKET s, struct sockaddr *name,
	int namelen, int msec_timeout );
typedef int (NSLDAPI_CONNECT_FN )( LBER_SOCKET s, struct sockaddr *name,
	int namelen );
typedef int (NSLDAPI_CLOSE_FN )( LBER_SOCKET s );

static int nsldapi_try_each_host( LDAP *ld, const char *hostlist, int defport,
	int secure, NSLDAPI_SOCKET_FN *socketfn, NSLDAPI_IOCTL_FN *ioctlfn,
	NSLDAPI_CONNECT_WITH_TO_FN *connectwithtofn,
	NSLDAPI_CONNECT_FN *connectfn, NSLDAPI_CLOSE_FN *closefn );


static int
nsldapi_os_closesocket( LBER_SOCKET s )
{
	int	rc;

#ifdef NSLDAPI_AVOID_OS_SOCKETS
	rc = -1;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
#ifdef _WINDOWS
	rc = closesocket( s );
#else /* _WINDOWS */
	rc = close( s );
#endif /* _WINDOWS */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */
	return( rc );
}


static LBER_SOCKET
nsldapi_os_socket( LDAP *ld, int secure, int domain, int type, int protocol )
{
#ifdef NSLDAPI_AVOID_OS_SOCKETS
	return -1;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
	int		s, invalid_socket;
	char		*errmsg = NULL;

	if ( secure ) {
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL,
			    nsldapi_strdup( "secure mode not supported" ));
		return( -1 );
	}

	s = socket( domain, type, protocol );

	/*
	 * if the socket() call failed or it returned a socket larger
	 * than we can deal with, return a "local error."
	 */
	if ( NSLDAPI_INVALID_OS_SOCKET( s )) {
		errmsg = "unable to create a socket";
		invalid_socket = 1;
	} else {	/* valid socket -- check for overflow */
		invalid_socket = 0;
#if !defined(NSLDAPI_HAVE_POLL) && !defined(_WINDOWS)
		/* not on Windows and do not have poll() */
		if ( s >= FD_SETSIZE ) {
			errmsg = "can't use socket >= FD_SETSIZE";
		}
#endif
	}

	if ( errmsg != NULL ) {	/* local socket error */
		if ( !invalid_socket ) {
			nsldapi_os_closesocket( s );
		}
		errmsg = nsldapi_strdup( errmsg );
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, errmsg );
		return( -1 );
	}

	return( s );
#endif /* NSLDAPI_AVOID_OS_SOCKETS */
}



/*
 * Non-blocking connect call function
 */
static int
nsldapi_os_connect_with_to(LBER_SOCKET sockfd, struct sockaddr *saptr,
	int salen, int msec)
{
#ifdef NSLDAPI_AVOID_OS_SOCKETS
	return -1;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
	int		n, error;
	int		len;
#if defined(_WINDOWS) || defined(XP_OS2)
	int		nonblock = 1;
	int		block = 0;
#else
	int		flags;
#endif /* _WINDOWS */
#ifdef NSLDAPI_HAVE_POLL
	struct pollfd   pfd;
#else
	struct timeval	tval;
	fd_set		rset, wset;
#ifdef _WINDOWS
	fd_set		eset;
#endif
#endif /* NSLDAPI_HAVE_POLL */


	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_connect_nonblock timeout: %d (msec)\n",
		msec, 0, 0);

#ifdef _WINDOWS
	ioctlsocket(sockfd, FIONBIO, &nonblock);
#elif defined(XP_OS2)
  ioctl( sockfd, FIONBIO, &nonblock, sizeof(nonblock) );
#else
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
#endif /* _WINDOWS */

	error = 0;
	if ((n = connect(sockfd, saptr, salen)) < 0)
#ifdef _WINDOWS
		if ((n != SOCKET_ERROR) &&  (WSAGetLastError() != WSAEWOULDBLOCK)) {
#else
		if (errno != EINPROGRESS) {
#endif /* _WINDOWS */
#ifdef LDAP_DEBUG
			if ( ldap_debug & LDAP_DEBUG_TRACE ) {
				perror("connect");
			}
#endif
			return (-1);
		}

	/* success */
	if (n == 0)
		goto done;

#ifdef NSLDAPI_HAVE_POLL
	pfd.fd = sockfd;
	pfd.events = POLLOUT;
#else
	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);
	wset = rset;

#ifdef _WINDOWS
	eset = rset;
#endif /* _WINDOWS */
#endif /* NSLDAPI_HAVE_POLL */

	if (msec < 0 && msec != LDAP_X_IO_TIMEOUT_NO_TIMEOUT) {
		LDAPDebug( LDAP_DEBUG_TRACE, "Invalid timeout value detected.."
			"resetting connect timeout to default value "
			"(LDAP_X_IO_TIMEOUT_NO_TIMEOUT\n", 0, 0, 0);
		msec = LDAP_X_IO_TIMEOUT_NO_TIMEOUT;
#ifndef NSLDAPI_HAVE_POLL
	} else {
		if (msec != 0) {
			tval.tv_sec = msec / 1000;
			tval.tv_usec = 1000 * ( msec % 1000 );
		} else {
			tval.tv_sec = 0;
			tval.tv_usec = 0;
		}
#endif /* NSLDAPI_HAVE_POLL */
	}

#ifdef NSLDAPI_HAVE_POLL
	if ((n = poll(&pfd, 1,
		(msec != LDAP_X_IO_TIMEOUT_NO_TIMEOUT) ? msec : -1)) == 0) {
		errno = ETIMEDOUT;
		return (-1);
	}
	if (pfd.revents & (POLLOUT|POLLERR|POLLHUP|POLLNVAL)) {
		len = sizeof(error);
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&error, &len)
			< 0)
			return (-1);
#ifdef LDAP_DEBUG
	} else if ( ldap_debug & LDAP_DEBUG_TRACE ) {
		perror("poll error: sockfd not set");
#endif
	}

#else /* NSLDAPI_HAVE_POLL */
	/* if timeval structure == NULL, select will block indefinitely */
	/* 			!= NULL, and value == 0, select will */
	/* 			         not block */
	/* Windows is a bit quirky on how it behaves w.r.t nonblocking */
	/* connects.  If the connect fails, the exception fd, eset, is */
	/* set to show the failure.  The first argument in select is */
	/* ignored */

#ifdef _WINDOWS
	if ((n = select(sockfd +1, &rset, &wset, &eset,
		(msec != LDAP_X_IO_TIMEOUT_NO_TIMEOUT) ? &tval : NULL)) == 0) {
		errno = WSAETIMEDOUT;
		return (-1);
	}
	/* if wset is set, the connect worked */
	if (FD_ISSET(sockfd, &wset) || FD_ISSET(sockfd, &rset)) {
		len = sizeof(error);
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&error, &len)
			< 0)
			return (-1);
		goto done;
	}

	/* if eset is set, the connect failed */
	if (FD_ISSET(sockfd, &eset)) {
		return (-1);
	}

	/* failure on select call */
	if (n == SOCKET_ERROR) {
		perror("select error: SOCKET_ERROR returned");
		return (-1);		
	}
#else
	if ((n = select(sockfd +1, &rset, &wset, NULL,
		(msec != LDAP_X_IO_TIMEOUT_NO_TIMEOUT) ? &tval : NULL)) == 0) {
		errno = ETIMEDOUT;
		return (-1);
	}
	if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
		len = sizeof(error);
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char *)&error, &len)
			< 0)
			return (-1);
#ifdef LDAP_DEBUG
	} else if ( ldap_debug & LDAP_DEBUG_TRACE ) {
		perror("select error: sockfd not set");
#endif
	}
#endif /* _WINDOWS */
#endif /* NSLDAPI_HAVE_POLL */

done:
#ifdef _WINDOWS
	ioctlsocket(sockfd, FIONBIO, &block);
#elif defined(XP_OS2)
  ioctl( sockfd, FIONBIO, &nonblock, sizeof(block) );
#else
	fcntl(sockfd, F_SETFL, flags);
#endif /* _WINDOWS */

	if (error) {
		errno = error;
		return (-1);
	}

	return (0);
#endif /* NSLDAPI_AVOID_OS_SOCKETS */
}


static int
nsldapi_os_ioctl( LBER_SOCKET s, int option, int *statusp )
{
#ifdef NSLDAPI_AVOID_OS_SOCKETS
	return -1;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
	int		err;
#if defined(_WINDOWS) || defined(XP_OS2)
	u_long		iostatus;
#endif

	if ( FIONBIO != option ) {
		return( -1 );
	}

#ifdef _WINDOWS
	iostatus = *(u_long *)statusp;
	err = ioctlsocket( s, FIONBIO, &iostatus );
#else
#ifdef XP_OS2
	err = ioctl( s, FIONBIO, (caddr_t)&iostatus, sizeof(iostatus) );
#else
	err = ioctl( s, FIONBIO, (caddr_t)statusp );
#endif
#endif

	return( err );
#endif /* NSLDAPI_AVOID_OS_SOCKETS */
}


int
nsldapi_connect_to_host( LDAP *ld, Sockbuf *sb, const char *hostlist,
		int defport, int secure, char **krbinstancep )
/*
 * "defport" must be in host byte order
 * zero is returned upon success, -1 if fatal error, -2 EINPROGRESS
 * if -1 is returned, ld_errno is set
 */
{
	int		s;

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_connect_to_host: %s, port: %d\n",
	    NULL == hostlist ? "NULL" : hostlist, defport, 0 );

	/*
	 * If an extended I/O connect callback has been defined, just use it.
	 */
	if ( NULL != ld->ld_extconnect_fn ) {
		unsigned long connect_opts = 0;

		if ( ld->ld_options & LDAP_BITOPT_ASYNC) {
			connect_opts |= LDAP_X_EXTIOF_OPT_NONBLOCKING;
		}
		if ( secure ) {
			connect_opts |= LDAP_X_EXTIOF_OPT_SECURE;
		}
		s = ld->ld_extconnect_fn( hostlist, defport,
		    ld->ld_connect_timeout, connect_opts,
		    ld->ld_ext_session_arg,
		    &sb->sb_ext_io_fns.lbextiofn_socket_arg );

	} else {
#ifdef NSLDAPI_AVOID_OS_SOCKETS
		return( -1 );
#else /* NSLDAPI_AVOID_OS_SOCKETS */
		s = nsldapi_try_each_host( ld, hostlist,
			defport, secure, nsldapi_os_socket,
			nsldapi_os_ioctl, nsldapi_os_connect_with_to,
			NULL, nsldapi_os_closesocket );
#endif /* NSLDAPI_AVOID_OS_SOCKETS */
	}

	if ( s < 0 ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONNECT_ERROR, NULL, NULL );
		return( -1 );
	}

	sb->sb_sd = s;

	/*
	 * Set krbinstancep (canonical name of host for use by Kerberos).
	 */
#ifdef KERBEROS
	char	*p;

	if (( *krbinstancep = nsldapi_host_connected_to( sb )) != NULL
	    && ( p = strchr( *krbinstancep, '.' )) != NULL ) {
		*p = '\0';
	}
#else /* KERBEROS */
	*krbinstancep = NULL;
#endif /* KERBEROS */

	return( 0 );
}


/*
 * Returns a socket number if successful and -1 if an error occurs.
 */
static int
nsldapi_try_each_host( LDAP *ld, const char *hostlist,
	int defport, int secure, NSLDAPI_SOCKET_FN *socketfn,
	NSLDAPI_IOCTL_FN *ioctlfn, NSLDAPI_CONNECT_WITH_TO_FN *connectwithtofn,
	NSLDAPI_CONNECT_FN *connectfn, NSLDAPI_CLOSE_FN *closefn )
{
#ifdef NSLDAPI_AVOID_OS_SOCKETS
	return -1;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
	int			rc = -1;
	int			s = 0;
	int			i, err, connected, use_hp;
	int			parse_err, port;
	struct sockaddr_in	sin;
	nsldapi_in_addr_t	address;
	char			**addrlist, *ldhpbuf, *ldhpbuf_allocd = NULL;
	char			*host;
	LDAPHostEnt		ldhent, *ldhp;
	struct hostent		*hp;
	struct ldap_x_hostlist_status	*status;
#ifdef GETHOSTBYNAME_BUF_T
	GETHOSTBYNAME_BUF_T	hbuf;
	struct hostent		hent;
#endif /* GETHOSTBYNAME_BUF_T */

	connected = 0;
	parse_err = ldap_x_hostlist_first( hostlist, defport, &host, &port,
            &status );
	while ( !connected && LDAP_SUCCESS == parse_err && host != NULL ) {
		ldhpbuf_allocd = NULL;
		ldhp = NULL;
		hp = NULL;
		s = 0;
		use_hp = 0;
		addrlist = NULL;


		if (( address = inet_addr( host )) == -1 ) {
			if ( ld->ld_dns_gethostbyname_fn == NULL ) {
#ifdef GETHOSTBYNAME_R_RETURNS_INT
				(void)GETHOSTBYNAME( host, &hent, hbuf,
				    sizeof(hbuf), &hp, &err );
#else
				hp = GETHOSTBYNAME( host, &hent, hbuf,
				    sizeof(hbuf), &err );
#endif
				if ( hp != NULL ) {
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
#endif /* else GETHOSTBYNAME_buf_t */

				if ( ldhpbuf == NULL ) {
					LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY,
					    NULL, NULL );
					ldap_memfree( host );
					ldap_x_hostlist_statusfree( status );
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
				ldap_memfree( host );
				ldap_x_hostlist_statusfree( status );
				return( -1 );
			}
			use_hp = 1;
		}

		rc = -1;
		for ( i = 0; !use_hp || ( addrlist[ i ] != 0 ); i++ ) {
			if ( -1 == ( s = (*socketfn)( ld, secure, AF_INET,
					SOCK_STREAM, 0 ))) {
				if ( ldhpbuf_allocd != NULL ) {
					NSLDAPI_FREE( ldhpbuf_allocd );
				}
				ldap_memfree( host );
				ldap_x_hostlist_statusfree( status );
				return( -1 );
			}

			if ( ld->ld_options & LDAP_BITOPT_ASYNC ) {
				int	iostatus = 1;

				err = (*ioctlfn)( s, FIONBIO, &iostatus );
				if ( err == -1 ) {
					LDAPDebug( LDAP_DEBUG_ANY,
					    "FIONBIO ioctl failed on %d\n",
					    s, 0, 0 );
				}
			}

			(void)memset( (char *)&sin, 0, sizeof( struct sockaddr_in ));
			sin.sin_family = AF_INET;
			sin.sin_port = htons( (unsigned short)port );

			SAFEMEMCPY( (char *) &sin.sin_addr.s_addr,
			    ( use_hp ? (char *) addrlist[ i ] :
			    (char *) &address ), sizeof( sin.sin_addr.s_addr) );

			{
#ifdef NSLDAPI_CONNECT_MUST_NOT_BE_INTERRUPTED
/*
 * Block all of the signals that might interrupt connect() since
 * there is an OS bug that causes connect() to fail if it is restarted.
 * Look in ../../include/portable.h for the definition of
 * NSLDAPI_CONNECT_MUST_NOT_BE_INTERRUPTED.
 */
				sigset_t	ints_off, oldset;

				sigemptyset( &ints_off );
				sigaddset( &ints_off, SIGALRM );
				sigaddset( &ints_off, SIGIO );
				sigaddset( &ints_off, SIGCLD );

				NSLDAPI_MT_SAFE_SIGPROCMASK( SIG_BLOCK, &ints_off, &oldset );
#endif /* NSLDAPI_CONNECT_MUST_NOT_BE_INTERRUPTED */

				if ( NULL != connectwithtofn  ) {	
					err = (*connectwithtofn)(s,
						(struct sockaddr *)&sin,
						sizeof(struct sockaddr_in),
						ld->ld_connect_timeout);
				} else {
					err = (*connectfn)(s,
						(struct sockaddr *)&sin,
						sizeof(struct sockaddr_in));
				}
#ifdef NSLDAPI_CONNECT_MUST_NOT_BE_INTERRUPTED
/*
 * restore original signal mask
 */
				NSLDAPI_MT_SAFE_SIGPROCMASK( SIG_SETMASK, &oldset, 0 );
#endif /* NSLDAPI_CONNECT_MUST_NOT_BE_INTERRUPTED */

			}
			if ( err >= 0 ) {
				connected = 1;
				rc = 0;
				break;
			} else {
				if ( ld->ld_options & LDAP_BITOPT_ASYNC) {
#ifdef _WINDOWS
					if (err == -1 && WSAGetLastError() == WSAEWOULDBLOCK)
						LDAP_SET_ERRNO( ld, EWOULDBLOCK );
#endif /* _WINDOWS */
					err = LDAP_GET_ERRNO( ld );
					if ( NSLDAPI_ERRNO_IO_INPROGRESS( err )) {
						LDAPDebug( LDAP_DEBUG_TRACE, "connect would block...\n",
							   0, 0, 0 );
						rc = -2;
						break;
					}
				}

#ifdef LDAP_DEBUG
				if ( ldap_debug & LDAP_DEBUG_TRACE ) {
					perror( (char *)inet_ntoa( sin.sin_addr ));
				}
#endif
				(*closefn)( s );
				if ( !use_hp ) {
					break;
				}
			}
		}

		ldap_memfree( host );
		parse_err = ldap_x_hostlist_next( &host, &port, status );
	}

	if ( ldhpbuf_allocd != NULL ) {
		NSLDAPI_FREE( ldhpbuf_allocd );
	}
	ldap_memfree( host );
	ldap_x_hostlist_statusfree( status );

	if ( connected ) {
		LDAPDebug( LDAP_DEBUG_TRACE, "sd %d connected to: %s\n",
		    s, inet_ntoa( sin.sin_addr ), 0 );
	}

	return( rc == 0 ? s : -1 );
#endif /* NSLDAPI_AVOID_OS_SOCKETS */
}

void
nsldapi_close_connection( LDAP *ld, Sockbuf *sb )
{
	if ( ld->ld_extclose_fn == NULL ) {
#ifndef NSLDAPI_AVOID_OS_SOCKETS
		nsldapi_os_closesocket( sb->sb_sd );
#endif /* NSLDAPI_AVOID_OS_SOCKETS */
	} else {
		ld->ld_extclose_fn( sb->sb_sd,
			    sb->sb_ext_io_fns.lbextiofn_socket_arg );
	}
}


#ifdef KERBEROS
char *
nsldapi_host_connected_to( LDAP *ld, Sockbuf *sb )
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
#error XXXmcs: need to use DNS callbacks here
	if (( hp = (struct hostent *)gethostbyaddr( (char *) &sin.sin_addr,
	    sizeof( sin.sin_addr ), AF_INET )) != NULL ) {
		if ( hp->h_name != NULL ) {
			return( nsldapi_strdup( hp->h_name ));
		}
	}

	return( NULL );
}
#endif /* KERBEROS */


/*
 * Returns 0 if all goes well and -1 if an error occurs (error code set in ld)
 * Also allocates initializes ld->ld_iostatus if needed..
 */
int
nsldapi_iostatus_interest_write( LDAP *ld, Sockbuf *sb )
{
	int		rc = 0;
	NSLDAPIIOStatus	*iosp;

	LDAP_MUTEX_LOCK( ld, LDAP_IOSTATUS_LOCK );

	if ( ld->ld_iostatus == NULL
	    && nsldapi_iostatus_init_nolock( ld ) < 0 ) {
		rc = -1;
		goto unlock_and_return;
	}

	iosp = ld->ld_iostatus;

	if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_OSNATIVE ) {
#ifdef NSLDAPI_AVOID_OS_SOCKETS
		rc = -1;
		goto unlock_and_return;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
#ifdef NSLDAPI_HAVE_POLL
		if ( nsldapi_add_to_os_pollfds( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo, POLLOUT )) {
			++iosp->ios_write_count;
		}
#else /* NSLDAPI_HAVE_POLL */
		if ( !FD_ISSET( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo.ossi_writefds )) {
			FD_SET( sb->sb_sd,
			    &iosp->ios_status.ios_osinfo.ossi_writefds );
			++iosp->ios_write_count;
		}
#endif /* else NSLDAPI_HAVE_POLL */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */

	} else if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_CALLBACK ) {
		if ( nsldapi_add_to_cb_pollfds( sb,
		    &iosp->ios_status.ios_cbinfo, LDAP_X_POLLOUT )) {
			++iosp->ios_write_count;
		}

	} else {
		LDAPDebug( LDAP_DEBUG_ANY,
		    "nsldapi_iostatus_interest_write: unknown I/O type %d\n",
		     iosp->ios_type, 0, 0 );
	}

unlock_and_return:
	LDAP_MUTEX_UNLOCK( ld, LDAP_IOSTATUS_LOCK );
	return( rc );
}


/*
 * Returns 0 if all goes well and -1 if an error occurs (error code set in ld)
 * Also allocates initializes ld->ld_iostatus if needed..
 */
int
nsldapi_iostatus_interest_read( LDAP *ld, Sockbuf *sb )
{
	int		rc = 0;
	NSLDAPIIOStatus	*iosp;

	LDAP_MUTEX_LOCK( ld, LDAP_IOSTATUS_LOCK );

	if ( ld->ld_iostatus == NULL
	    && nsldapi_iostatus_init_nolock( ld ) < 0 ) {
		rc = -1;
		goto unlock_and_return;
	}

	iosp = ld->ld_iostatus;

	if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_OSNATIVE ) {
#ifdef NSLDAPI_AVOID_OS_SOCKETS
		rc = -1;
		goto unlock_and_return;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
#ifdef NSLDAPI_HAVE_POLL
		if ( nsldapi_add_to_os_pollfds( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo, POLLIN )) {
			++iosp->ios_read_count;
		}
#else /* NSLDAPI_HAVE_POLL */
		if ( !FD_ISSET( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo.ossi_readfds )) {
			FD_SET( sb->sb_sd,
			    &iosp->ios_status.ios_osinfo.ossi_readfds );
			++iosp->ios_read_count;
		}
#endif /* else NSLDAPI_HAVE_POLL */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */

	} else if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_CALLBACK ) {
		if ( nsldapi_add_to_cb_pollfds( sb,
		    &iosp->ios_status.ios_cbinfo, LDAP_X_POLLIN )) {
			++iosp->ios_read_count;
		}
	} else {
		LDAPDebug( LDAP_DEBUG_ANY,
		    "nsldapi_iostatus_interest_read: unknown I/O type %d\n",
		     iosp->ios_type, 0, 0 );
	}

unlock_and_return:
	LDAP_MUTEX_UNLOCK( ld, LDAP_IOSTATUS_LOCK );
	return( rc );
}


/*
 * Returns 0 if all goes well and -1 if an error occurs (error code set in ld)
 * Also allocates initializes ld->ld_iostatus if needed..
 */
int
nsldapi_iostatus_interest_clear( LDAP *ld, Sockbuf *sb )
{
	int		rc = 0;
	NSLDAPIIOStatus	*iosp;

	LDAP_MUTEX_LOCK( ld, LDAP_IOSTATUS_LOCK );

	if ( ld->ld_iostatus == NULL
	    && nsldapi_iostatus_init_nolock( ld ) < 0 ) {
		rc = -1;
		goto unlock_and_return;
	}

	iosp = ld->ld_iostatus;

	if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_OSNATIVE ) {
#ifdef NSLDAPI_AVOID_OS_SOCKETS
		rc = -1;
		goto unlock_and_return;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
#ifdef NSLDAPI_HAVE_POLL
		if ( nsldapi_clear_from_os_pollfds( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo, POLLOUT )) {
			--iosp->ios_write_count;
		}
		if ( nsldapi_clear_from_os_pollfds( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo, POLLIN )) {
			--iosp->ios_read_count;
		}
#else /* NSLDAPI_HAVE_POLL */
		if ( FD_ISSET( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo.ossi_writefds )) {
			FD_CLR( sb->sb_sd,
			    &iosp->ios_status.ios_osinfo.ossi_writefds );
			--iosp->ios_write_count;
		}
		if ( FD_ISSET( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo.ossi_readfds )) {
			FD_CLR( sb->sb_sd,
			    &iosp->ios_status.ios_osinfo.ossi_readfds );
			--iosp->ios_read_count;
		}
#endif /* else NSLDAPI_HAVE_POLL */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */

	} else if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_CALLBACK ) {
		if ( nsldapi_clear_from_cb_pollfds( sb,
		    &iosp->ios_status.ios_cbinfo, LDAP_X_POLLOUT )) {
			--iosp->ios_write_count;
		}
		if ( nsldapi_clear_from_cb_pollfds( sb,
		    &iosp->ios_status.ios_cbinfo, LDAP_X_POLLIN )) {
			--iosp->ios_read_count;
		}
	} else {
		LDAPDebug( LDAP_DEBUG_ANY,
		    "nsldapi_iostatus_interest_clear: unknown I/O type %d\n",
		     iosp->ios_type, 0, 0 );
	}

unlock_and_return:
	LDAP_MUTEX_UNLOCK( ld, LDAP_IOSTATUS_LOCK );
	return( rc );
}


/*
 * Return a non-zero value if sb is ready for write.
 */
int
nsldapi_iostatus_is_write_ready( LDAP *ld, Sockbuf *sb )
{
	int		rc = 0;
	NSLDAPIIOStatus	*iosp;

	LDAP_MUTEX_LOCK( ld, LDAP_IOSTATUS_LOCK );
	iosp = ld->ld_iostatus;
	if ( iosp == NULL ) {
		goto unlock_and_return;
	}

	if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_OSNATIVE ) {
#ifdef NSLDAPI_AVOID_OS_SOCKETS
		goto unlock_and_return;
#else  /* NSLDAPI_AVOID_OS_SOCKETS */
#ifdef NSLDAPI_HAVE_POLL
		/*
		 * if we are using poll() we do something a little tricky: if
		 * any bits in the socket's returned events field other than
		 * POLLIN (ready for read) are set, we return true.  This
		 * is done so we notice when a server closes a connection
		 * or when another error occurs.  The actual error will be
		 * noticed later when we call write() or send().
		 */
		rc = nsldapi_find_in_os_pollfds( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo, ~POLLIN );

#else /* NSLDAPI_HAVE_POLL */
		rc = FD_ISSET( sb->sb_sd,
			&iosp->ios_status.ios_osinfo.ossi_use_writefds );
#endif /* else NSLDAPI_HAVE_POLL */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */

	} else if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_CALLBACK ) {
		rc = nsldapi_find_in_cb_pollfds( sb,
		    &iosp->ios_status.ios_cbinfo, ~LDAP_X_POLLIN );

	} else {
		LDAPDebug( LDAP_DEBUG_ANY,
		    "nsldapi_iostatus_is_write_ready: unknown I/O type %d\n",
		     iosp->ios_type, 0, 0 );
		rc = 0;
	}

unlock_and_return:
	LDAP_MUTEX_UNLOCK( ld, LDAP_IOSTATUS_LOCK );
	return( rc );
}


/*
 * Return a non-zero value if sb is ready for read.
 */
int
nsldapi_iostatus_is_read_ready( LDAP *ld, Sockbuf *sb )
{
	int		rc = 0;
	NSLDAPIIOStatus	*iosp;

	LDAP_MUTEX_LOCK( ld, LDAP_IOSTATUS_LOCK );
	iosp = ld->ld_iostatus;
	if ( iosp == NULL ) {
		goto unlock_and_return;
	}

	if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_OSNATIVE ) {
#ifdef NSLDAPI_AVOID_OS_SOCKETS
		goto unlock_and_return;
#else /* NSLDAPI_AVOID_OS_SOCKETS */
#ifdef NSLDAPI_HAVE_POLL
		/*
		 * if we are using poll() we do something a little tricky: if
		 * any bits in the socket's returned events field other than
		 * POLLOUT (ready for write) are set, we return true.  This
		 * is done so we notice when a server closes a connection
		 * or when another error occurs.  The actual error will be
		 * noticed later when we call read() or recv().
		 */
		rc = nsldapi_find_in_os_pollfds( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo, ~POLLOUT );

#else /* NSLDAPI_HAVE_POLL */
		rc = FD_ISSET( sb->sb_sd,
		    &iosp->ios_status.ios_osinfo.ossi_use_readfds );
#endif /* else NSLDAPI_HAVE_POLL */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */

	} else if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_CALLBACK ) {
		rc = nsldapi_find_in_cb_pollfds( sb,
		    &iosp->ios_status.ios_cbinfo, ~LDAP_X_POLLOUT );

	} else {
		LDAPDebug( LDAP_DEBUG_ANY,
		    "nsldapi_iostatus_is_read_ready: unknown I/O type %d\n",
		     iosp->ios_type, 0, 0 );
		rc = 0;
	}

unlock_and_return:
	LDAP_MUTEX_UNLOCK( ld, LDAP_IOSTATUS_LOCK );
	return( rc );
}


/*
 * Allocate and initialize ld->ld_iostatus if not already done.
 * Should be called with LDAP_IOSTATUS_LOCK locked.
 * Returns 0 if all goes well and -1 if not (sets error in ld)
 */
static int
nsldapi_iostatus_init_nolock( LDAP *ld )
{
	NSLDAPIIOStatus	*iosp;

	if ( ld->ld_iostatus != NULL ) {
		return( 0 );
	}

	if (( iosp = (NSLDAPIIOStatus *)NSLDAPI_CALLOC( 1,
	    sizeof( NSLDAPIIOStatus ))) == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( -1 );
	}

	if ( ld->ld_extpoll_fn == NULL ) {
		iosp->ios_type = NSLDAPI_IOSTATUS_TYPE_OSNATIVE;
#ifdef NSLDAPI_AVOID_OS_SOCKETS
		return( -1 );
#else /* NSLDAPI_AVOID_OS_SOCKETS */
#ifndef NSLDAPI_HAVE_POLL
		FD_ZERO( &iosp->ios_status.ios_osinfo.ossi_readfds );
		FD_ZERO( &iosp->ios_status.ios_osinfo.ossi_writefds );
#endif /* !NSLDAPI_HAVE_POLL */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */

	} else {
		iosp->ios_type = NSLDAPI_IOSTATUS_TYPE_CALLBACK;
	}

	ld->ld_iostatus = iosp;
	return( 0 );
}


void
nsldapi_iostatus_free( LDAP *ld )
{
	if ( ld == NULL ) {
		return;
	}

		
	/* clean up classic I/O compatibility glue */
	if ( ld->ld_io_fns_ptr != NULL ) {
		if ( ld->ld_ext_session_arg != NULL ) {
			NSLDAPI_FREE( ld->ld_ext_session_arg );
		}
		NSLDAPI_FREE( ld->ld_io_fns_ptr );
	}

	/* clean up I/O status tracking info. */
	if ( ld->ld_iostatus != NULL ) {
		NSLDAPIIOStatus	*iosp = ld->ld_iostatus;

		if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_OSNATIVE ) {
#ifdef NSLDAPI_HAVE_POLL
			if ( iosp->ios_status.ios_osinfo.ossi_pollfds
			    != NULL ) {
				NSLDAPI_FREE(
				    iosp->ios_status.ios_osinfo.ossi_pollfds );
			}
#endif /* NSLDAPI_HAVE_POLL */

		} else if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_CALLBACK ) {
			if ( iosp->ios_status.ios_cbinfo.cbsi_pollfds
			    != NULL ) {
				NSLDAPI_FREE(
				    iosp->ios_status.ios_cbinfo.cbsi_pollfds );
			}
		} else {
			LDAPDebug( LDAP_DEBUG_ANY,
			    "nsldapi_iostatus_free: unknown I/O type %d\n",
			     iosp->ios_type, 0, 0 );
		}

		NSLDAPI_FREE( iosp );
	}
}



#if !defined(NSLDAPI_HAVE_POLL) && !defined(NSLDAPI_AVOID_OS_SOCKETS)
static int
nsldapi_get_select_table_size( void )
{
	static int	tblsize = 0;	/* static */

	if ( tblsize == 0 ) {
#if defined(_WINDOWS) || defined(XP_OS2)
		tblsize = FOPEN_MAX; /* ANSI spec. */
#else
#ifdef USE_SYSCONF
		tblsize = sysconf( _SC_OPEN_MAX );
#else /* USE_SYSCONF */
		tblsize = getdtablesize();
#endif /* else USE_SYSCONF */
#endif /* else _WINDOWS */

		if ( tblsize >= FD_SETSIZE ) {
			/*
			 * clamp value so we don't overrun the fd_set structure
			 */
			tblsize = FD_SETSIZE - 1;
		}
	}

	return( tblsize );
}
#endif /* !defined(NSLDAPI_HAVE_POLL) && !defined(NSLDAPI_AVOID_OS_SOCKETS) */


static int
nsldapi_tv2ms( struct timeval *tv )
{
	if ( tv == NULL ) {
		return( -1 );	/* infinite timout for poll() */
	}

	return( tv->tv_sec * 1000 + tv->tv_usec / 1000 );
}


int
nsldapi_iostatus_poll( LDAP *ld, struct timeval *timeout )
{
	int			rc;
	NSLDAPIIOStatus		*iosp;

	LDAPDebug( LDAP_DEBUG_TRACE, "nsldapi_iostatus_poll\n", 0, 0, 0 );

	LDAP_MUTEX_LOCK( ld, LDAP_IOSTATUS_LOCK );
	iosp = ld->ld_iostatus;

	if ( iosp == NULL ||
            ( iosp->ios_read_count <= 0 && iosp->ios_write_count <= 0 )) {
		rc = 0;		/* simulate a timeout */

	} else if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_OSNATIVE ) {
#ifndef NSLDAPI_AVOID_OS_SOCKETS
#ifdef NSLDAPI_HAVE_POLL

		rc = NSLDAPI_POLL( iosp->ios_status.ios_osinfo.ossi_pollfds,
		    iosp->ios_status.ios_osinfo.ossi_pollfds_size,
		    nsldapi_tv2ms( timeout ));

#else /* NSLDAPI_HAVE_POLL */

		/* two (potentially large) struct copies */
		iosp->ios_status.ios_osinfo.ossi_use_readfds
		    = iosp->ios_status.ios_osinfo.ossi_readfds;
		iosp->ios_status.ios_osinfo.ossi_use_writefds
		    = iosp->ios_status.ios_osinfo.ossi_writefds;

#ifdef HPUX9
		rc = NSLDAPI_SELECT( nsldapi_get_select_table_size(),
		    (int *)&iosp->ios_status.ios_osinfo.ossi_use_readfds
		    (int *)&iosp->ios_status.ios_osinfo.ossi_use_writefds,
		    NULL, timeout );
#else
		rc = NSLDAPI_SELECT( nsldapi_get_select_table_size(),
		    &iosp->ios_status.ios_osinfo.ossi_use_readfds,
		    &iosp->ios_status.ios_osinfo.ossi_use_writefds,
		    NULL, timeout );
#endif /* else HPUX9 */
#endif /* else NSLDAPI_HAVE_POLL */
#endif /* NSLDAPI_AVOID_OS_SOCKETS */

	} else if ( iosp->ios_type == NSLDAPI_IOSTATUS_TYPE_CALLBACK ) {
		/*
		 * We always pass the session extended I/O argument to
		 * the extended poll() callback.
		 */
		rc = ld->ld_extpoll_fn( 
		    iosp->ios_status.ios_cbinfo.cbsi_pollfds,
		    iosp->ios_status.ios_cbinfo.cbsi_pollfds_size,
		    nsldapi_tv2ms( timeout ), ld->ld_ext_session_arg );

	} else {
		LDAPDebug( LDAP_DEBUG_ANY,
		    "nsldapi_iostatus_poll: unknown I/O type %d\n",
		     iosp->ios_type, 0, 0 );
		rc = 0;	/* simulate a timeout (what else to do?) */
	}

	LDAP_MUTEX_UNLOCK( ld, LDAP_IOSTATUS_LOCK );
	return( rc );
}

#if defined(NSLDAPI_HAVE_POLL) && !defined(NSLDAPI_AVOID_OS_SOCKETS)
/*
 * returns 1 if "fd" was added to pollfds.
 * returns 1 if some of the bits in "events" were added to pollfds.
 * returns 0 if no changes were made.
 */
static int
nsldapi_add_to_os_pollfds( int fd, struct nsldapi_os_statusinfo *pip,
	short events )
{
	int	i, openslot;

	/* first we check to see if "fd" is already in our pollfds */
	openslot = -1;
	for ( i = 0; i < pip->ossi_pollfds_size; ++i ) {
		if ( pip->ossi_pollfds[ i ].fd == fd ) {
			if (( pip->ossi_pollfds[ i ].events & events )
			    != events ) {
				pip->ossi_pollfds[ i ].events |= events;
				return( 1 );
			} else {
				return( 0 );
			}
		}
		if ( pip->ossi_pollfds[ i ].fd == -1 && openslot == -1 ) {
			openslot = i;	/* remember for later */
		}
	}

	/*
	 * "fd" is not currently being poll'd on -- add to array.
	 * if we need to expand the pollfds array, we do it in increments of
	 * NSLDAPI_POLL_ARRAY_GROWTH (#define near the top of this file).
	 */
	if ( openslot == -1 ) {
		struct pollfd	*newpollfds;

		if ( pip->ossi_pollfds_size == 0 ) {
			newpollfds = (struct pollfd *)NSLDAPI_MALLOC(
			    NSLDAPI_POLL_ARRAY_GROWTH
			    * sizeof( struct pollfd ));
		} else {
			newpollfds = (struct pollfd *)NSLDAPI_REALLOC(
			    pip->ossi_pollfds, (NSLDAPI_POLL_ARRAY_GROWTH
			    + pip->ossi_pollfds_size)
			    * sizeof( struct pollfd ));
		}
		if ( newpollfds == NULL ) { /* XXXmcs: no way to return err! */
			return( 0 );
		}
		pip->ossi_pollfds = newpollfds;
		openslot = pip->ossi_pollfds_size;
		pip->ossi_pollfds_size += NSLDAPI_POLL_ARRAY_GROWTH;
		for ( i = openslot + 1; i < pip->ossi_pollfds_size; ++i ) {
			pip->ossi_pollfds[ i ].fd = -1;
			pip->ossi_pollfds[ i ].events =
			    pip->ossi_pollfds[ i ].revents = 0;
		}
	}
	pip->ossi_pollfds[ openslot ].fd = fd;
	pip->ossi_pollfds[ openslot ].events = events;
	pip->ossi_pollfds[ openslot ].revents = 0;
	return( 1 );
}


/*
 * returns 1 if any "events" from "fd" were removed from pollfds
 * returns 0 of "fd" wasn't in pollfds or if events did not overlap.
 */
static int
nsldapi_clear_from_os_pollfds( int fd, struct nsldapi_os_statusinfo *pip,
    short events )
{
	int	i;

	for ( i = 0; i < pip->ossi_pollfds_size; ++i ) {
		if ( pip->ossi_pollfds[i].fd == fd ) {
			if (( pip->ossi_pollfds[ i ].events & events ) != 0 ) {
				pip->ossi_pollfds[ i ].events &= ~events;
				if ( pip->ossi_pollfds[ i ].events == 0 ) {
					pip->ossi_pollfds[i].fd = -1;
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
nsldapi_find_in_os_pollfds( int fd, struct nsldapi_os_statusinfo *pip,
	short revents )
{
	int	i;

	for ( i = 0; i < pip->ossi_pollfds_size; ++i ) {
		if ( pip->ossi_pollfds[i].fd == fd ) {
			if (( pip->ossi_pollfds[ i ].revents & revents ) != 0 ) {
				return( 1 );	/* revents overlap */
			} else {
				return( 0 );	/* revents do not overlap */
			}
		}
	}

	return( 0 );	/* "fd" was not found */
}
#endif /* !defined(NSLDAPI_HAVE_POLL) && !defined(NSLDAPI_AVOID_OS_SOCKETS) */

/*
 * returns 1 if "sb" was added to pollfds.
 * returns 1 if some of the bits in "events" were added to pollfds.
 * returns 0 if no changes were made.
 */
static int
nsldapi_add_to_cb_pollfds( Sockbuf *sb, struct nsldapi_cb_statusinfo *pip,
    short events )
{
	int	i, openslot;

	/* first we check to see if "sb" is already in our pollfds */
	openslot = -1;
	for ( i = 0; i < pip->cbsi_pollfds_size; ++i ) {
		if ( NSLDAPI_CB_POLL_MATCH( sb, pip->cbsi_pollfds[ i ] )) {
			if (( pip->cbsi_pollfds[ i ].lpoll_events & events )
			    != events ) {
				pip->cbsi_pollfds[ i ].lpoll_events |= events;
				return( 1 );
			} else {
				return( 0 );
			}
		}
		if ( pip->cbsi_pollfds[ i ].lpoll_fd == -1 && openslot == -1 ) {
			openslot = i;	/* remember for later */
		}
	}

	/*
	 * "sb" is not currently being poll'd on -- add to array.
	 * if we need to expand the pollfds array, we do it in increments of
	 * NSLDAPI_POLL_ARRAY_GROWTH (#define near the top of this file).
	 */
	if ( openslot == -1 ) {
		LDAP_X_PollFD	*newpollfds;

		if ( pip->cbsi_pollfds_size == 0 ) {
			newpollfds = (LDAP_X_PollFD *)NSLDAPI_MALLOC(
			    NSLDAPI_POLL_ARRAY_GROWTH
			    * sizeof( LDAP_X_PollFD ));
		} else {
			newpollfds = (LDAP_X_PollFD *)NSLDAPI_REALLOC(
			    pip->cbsi_pollfds, (NSLDAPI_POLL_ARRAY_GROWTH
			    + pip->cbsi_pollfds_size)
			    * sizeof( LDAP_X_PollFD ));
		}
		if ( newpollfds == NULL ) { /* XXXmcs: no way to return err! */
			return( 0 );
		}
		pip->cbsi_pollfds = newpollfds;
		openslot = pip->cbsi_pollfds_size;
		pip->cbsi_pollfds_size += NSLDAPI_POLL_ARRAY_GROWTH;
		for ( i = openslot + 1; i < pip->cbsi_pollfds_size; ++i ) {
			pip->cbsi_pollfds[ i ].lpoll_fd = -1;
			pip->cbsi_pollfds[ i ].lpoll_socketarg = NULL;
			pip->cbsi_pollfds[ i ].lpoll_events =
			    pip->cbsi_pollfds[ i ].lpoll_revents = 0;
		}
	}
	pip->cbsi_pollfds[ openslot ].lpoll_fd = sb->sb_sd;
	pip->cbsi_pollfds[ openslot ].lpoll_socketarg =
	    sb->sb_ext_io_fns.lbextiofn_socket_arg;
	pip->cbsi_pollfds[ openslot ].lpoll_events = events;
	pip->cbsi_pollfds[ openslot ].lpoll_revents = 0;
	return( 1 );
}


/*
 * returns 1 if any "events" from "sb" were removed from pollfds
 * returns 0 of "sb" wasn't in pollfds or if events did not overlap.
 */
static int
nsldapi_clear_from_cb_pollfds( Sockbuf *sb,
    struct nsldapi_cb_statusinfo *pip, short events )
{
	int	i;

	for ( i = 0; i < pip->cbsi_pollfds_size; ++i ) {
		if ( NSLDAPI_CB_POLL_MATCH( sb, pip->cbsi_pollfds[ i ] )) {
			if (( pip->cbsi_pollfds[ i ].lpoll_events
			    & events ) != 0 ) {
				pip->cbsi_pollfds[ i ].lpoll_events &= ~events;
				if ( pip->cbsi_pollfds[ i ].lpoll_events
				    == 0 ) {
					pip->cbsi_pollfds[i].lpoll_fd = -1;
				}
				return( 1 );	/* events overlap */
			} else {
				return( 0 );	/* events do not overlap */
			}
		}
	}

	return( 0 );	/* "sb" was not found */
}


/*
 * returns 1 if any "revents" from "sb" were set in pollfds revents field.
 * returns 0 if not.
 */
static int
nsldapi_find_in_cb_pollfds( Sockbuf *sb, struct nsldapi_cb_statusinfo *pip,
    short revents )
{
	int	i;

	for ( i = 0; i < pip->cbsi_pollfds_size; ++i ) {
		if ( NSLDAPI_CB_POLL_MATCH( sb, pip->cbsi_pollfds[ i ] )) {
			if (( pip->cbsi_pollfds[ i ].lpoll_revents
			    & revents ) != 0 ) {
				return( 1 );	/* revents overlap */
			} else {
				return( 0 );	/* revents do not overlap */
			}
		}
	}

	return( 0 );	/* "sb" was not found */
}


/*
 * Install read and write functions into lber layer / sb
 */
int
nsldapi_install_lber_extiofns( LDAP *ld, Sockbuf *sb )
{
	struct lber_x_ext_io_fns	lberiofns;

	if ( NULL != sb ) {
		lberiofns.lbextiofn_size = LBER_X_EXTIO_FNS_SIZE;
		lberiofns.lbextiofn_read = ld->ld_extread_fn;
		lberiofns.lbextiofn_write = ld->ld_extwrite_fn;
		lberiofns.lbextiofn_writev = ld->ld_extwritev_fn;
		lberiofns.lbextiofn_socket_arg = ld->ld_ext_session_arg;

		if ( ber_sockbuf_set_option( sb, LBER_SOCKBUF_OPT_EXT_IO_FNS,
		    &lberiofns ) != 0 ) {
			return( LDAP_LOCAL_ERROR );
		}
	}

	return( LDAP_SUCCESS );
}


/*
 ******************************************************************************
 * One struct and several functions to bridge the gap between new extended
 * I/O functions that are installed using ldap_set_option( ...,
 * LDAP_OPT_EXTIO_FN_PTRS, ... ) and the original "classic" I/O functions
 * (installed using LDAP_OPT_IO_FN_PTRS) follow.
 *
 * Our basic strategy is to use the new extended arg to hold a pointer to a
 *    structure that contains a pointer to the LDAP * (which contains pointers
 *    to the old functions so we can call them) as well as a pointer to an
 *    LBER_SOCKET to hold the socket used by the classic functions (the new
 *    functions use a simple int for the socket).
 */
typedef struct nsldapi_compat_socket_info {
    LBER_SOCKET		csi_socket;
    LDAP		*csi_ld;
} NSLDAPICompatSocketInfo;
    
static int LDAP_CALLBACK
nsldapi_ext_compat_read( int s, void *buf, int len,
	struct lextiof_socket_private *arg )
{
	NSLDAPICompatSocketInfo	*csip = (NSLDAPICompatSocketInfo *)arg;
	struct ldap_io_fns	*iofns = csip->csi_ld->ld_io_fns_ptr;

	return( iofns->liof_read( csip->csi_socket, buf, len ));
}


static int LDAP_CALLBACK
nsldapi_ext_compat_write( int s, const void *buf, int len,
	struct lextiof_socket_private *arg  )
{
	NSLDAPICompatSocketInfo	*csip = (NSLDAPICompatSocketInfo *)arg;
	struct ldap_io_fns	*iofns = csip->csi_ld->ld_io_fns_ptr;

	return( iofns->liof_write( csip->csi_socket, buf, len ));
}


static int LDAP_CALLBACK
nsldapi_ext_compat_poll( LDAP_X_PollFD fds[], int nfds, int timeout,
	struct lextiof_session_private *arg )
{
	NSLDAPICompatSocketInfo	*csip = (NSLDAPICompatSocketInfo *)arg;
	struct ldap_io_fns	*iofns = csip->csi_ld->ld_io_fns_ptr;
	fd_set			readfds, writefds;
	int			i, rc, maxfd = 0;
	struct timeval		tv, *tvp;

	/*
	 * Prepare fd_sets for select()
	 */
	FD_ZERO( &readfds );
	FD_ZERO( &writefds );
	for ( i = 0; i < nfds; ++i ) {
		if ( fds[ i ].lpoll_fd < 0 ) {
			continue;
		}

		if ( fds[ i ].lpoll_fd >= FD_SETSIZE ) {
			LDAP_SET_ERRNO( csip->csi_ld, EINVAL );
			return( -1 );
		}
		
		if ( 0 != ( fds[i].lpoll_events & LDAP_X_POLLIN )) {
			FD_SET( fds[i].lpoll_fd, &readfds );
		}

		if ( 0 != ( fds[i].lpoll_events & LDAP_X_POLLOUT )) {
			FD_SET( fds[i].lpoll_fd, &writefds );
		}

		fds[i].lpoll_revents = 0;	/* clear revents */

		if ( fds[i].lpoll_fd >= maxfd ) {
			maxfd = fds[i].lpoll_fd;
		}
	}

	/*
	 * select() using callback.
	 */
	++maxfd;
	if ( timeout == -1 ) {
		tvp = NULL;
	} else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = 1000 * ( timeout - tv.tv_sec * 1000 );
		tvp = &tv;
	}
	rc = iofns->liof_select( maxfd, &readfds, &writefds, NULL, tvp );
	if ( rc <= 0 ) {	/* timeout or fatal error */
		return( rc );
	}

	/*
	 * Use info. in fd_sets to populate poll() revents.
	 */
	for ( i = 0; i < nfds; ++i ) {
		if ( fds[ i ].lpoll_fd < 0 ) {
			continue;
		}

		if ( 0 != ( fds[i].lpoll_events & LDAP_X_POLLIN )
		    && FD_ISSET( fds[i].lpoll_fd, &readfds )) {
			fds[i].lpoll_revents |= LDAP_X_POLLIN;
		}

		if ( 0 != ( fds[i].lpoll_events & LDAP_X_POLLOUT )
		    && FD_ISSET( fds[i].lpoll_fd, &writefds )) {
			fds[i].lpoll_revents |= LDAP_X_POLLOUT;
		}

		/* XXXmcs: any other cases to deal with?  LDAP_X_POLLERR? */
	}

	return( rc );
}


static LBER_SOCKET
nsldapi_compat_socket( LDAP *ld, int secure, int domain, int type,
	int protocol )
{
	int		s;

	s = ld->ld_io_fns_ptr->liof_socket( domain, type, protocol );

	if ( s >= 0 ) {
		char				*errmsg = NULL;

#ifdef NSLDAPI_HAVE_POLL
		if ( ld->ld_io_fns_ptr->liof_select != NULL
			    && s >= FD_SETSIZE ) {
			errmsg = "can't use socket >= FD_SETSIZE";
		}
#elif !defined(_WINDOWS) /* not on Windows and do not have poll() */
		if ( s >= FD_SETSIZE ) {
			errmsg = "can't use socket >= FD_SETSIZE";
		}
#endif

		if ( NULL == errmsg && secure &&
			    ld->ld_io_fns_ptr->liof_ssl_enable( s ) < 0 ) {
			errmsg = "failed to enable secure mode";
		    }

		if ( NULL != errmsg ) {
			if ( NULL == ld->ld_io_fns_ptr->liof_close ) {
				nsldapi_os_closesocket( s );
			} else {
				ld->ld_io_fns_ptr->liof_close( s );
			}
			LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL,
				    nsldapi_strdup( errmsg ));
			return( -1 );
		}
	}

	return( s );
}


/*
 * Note: timeout is ignored because we have no way to pass it via
 * the old I/O callback interface.
 */
static int LDAP_CALLBACK
nsldapi_ext_compat_connect( const char *hostlist, int defport, int timeout,
	unsigned long options, struct lextiof_session_private *sessionarg,
	struct lextiof_socket_private **socketargp )
{
	NSLDAPICompatSocketInfo		*defcsip;
	struct ldap_io_fns		*iofns;
	int				s, secure;
	NSLDAPI_SOCKET_FN		*socketfn;
	NSLDAPI_IOCTL_FN		*ioctlfn;
	NSLDAPI_CONNECT_WITH_TO_FN	*connectwithtofn;
	NSLDAPI_CONNECT_FN		*connectfn;
	NSLDAPI_CLOSE_FN		*closefn;

	defcsip = (NSLDAPICompatSocketInfo *)sessionarg;
	iofns = defcsip->csi_ld->ld_io_fns_ptr;

	if ( 0 != ( options & LDAP_X_EXTIOF_OPT_SECURE )) {
		if ( NULL == iofns->liof_ssl_enable ) {
			LDAP_SET_ERRNO( defcsip->csi_ld, EINVAL );
			return( -1 );
		}
		secure = 1;
	} else {
		secure = 0;
	}

	socketfn = ( iofns->liof_socket == NULL ) ?
		    nsldapi_os_socket : nsldapi_compat_socket;
	ioctlfn = ( iofns->liof_ioctl == NULL ) ?
		    nsldapi_os_ioctl : (NSLDAPI_IOCTL_FN *)(iofns->liof_ioctl);
	if ( NULL == iofns->liof_connect ) {
		connectwithtofn = nsldapi_os_connect_with_to;
		connectfn = NULL;
	} else {
		connectwithtofn = NULL;
		connectfn = iofns->liof_connect;
	}
	closefn = ( iofns->liof_close == NULL ) ?
		    nsldapi_os_closesocket : iofns->liof_close;	

	s = nsldapi_try_each_host( defcsip->csi_ld, hostlist, defport,
			secure, socketfn, ioctlfn, connectwithtofn,
			connectfn, closefn );

	if ( s >= 0 ) {
		NSLDAPICompatSocketInfo		*csip;

		if (( csip = (NSLDAPICompatSocketInfo *)NSLDAPI_CALLOC( 1,
		    sizeof( NSLDAPICompatSocketInfo ))) == NULL ) {
			(*closefn)( s );
			LDAP_SET_LDERRNO( defcsip->csi_ld, LDAP_NO_MEMORY,
				NULL, NULL );
			return( -1 );
		}

		csip->csi_socket = s;
		csip->csi_ld = defcsip->csi_ld;
		*socketargp = (void *)csip;

		/*
		 * We always return 1, which is a valid but not unique socket
		 * (file descriptor) number.  The extended I/O functions only
		 * require that the combination of the void *arg and the int
		 * socket be unique.  Since we allocate the
		 * NSLDAPICompatSocketInfo that we assign to arg, we meet
		 * that requirement.
		 */
		s = 1;
	}

	return( s );
}


static int LDAP_CALLBACK
nsldapi_ext_compat_close( int s, struct lextiof_socket_private *arg )
{
	NSLDAPICompatSocketInfo	*csip = (NSLDAPICompatSocketInfo *)arg;
	struct ldap_io_fns	*iofns = csip->csi_ld->ld_io_fns_ptr;
	int			rc;

	rc = iofns->liof_close( csip->csi_socket );

	NSLDAPI_FREE( csip );

	return( rc );
}

/*
 * Install the I/O functions.
 * Return an LDAP error code (LDAP_SUCCESS if all goes well).
 */
int
nsldapi_install_compat_io_fns( LDAP *ld, struct ldap_io_fns *iofns )
{
	NSLDAPICompatSocketInfo		*defcsip;

	if (( defcsip = (NSLDAPICompatSocketInfo *)NSLDAPI_CALLOC( 1,
	    sizeof( NSLDAPICompatSocketInfo ))) == NULL ) {
		return( LDAP_NO_MEMORY );
	}

	defcsip->csi_socket = -1;
	defcsip->csi_ld = ld;

	if ( ld->ld_io_fns_ptr != NULL ) {
		(void)memset( (char *)ld->ld_io_fns_ptr, 0,
		    sizeof( struct ldap_io_fns ));
	} else if (( ld->ld_io_fns_ptr = (struct ldap_io_fns *)NSLDAPI_CALLOC(
	    1, sizeof( struct ldap_io_fns ))) == NULL ) {
		NSLDAPI_FREE( defcsip );
		return( LDAP_NO_MEMORY );
	}

	/* struct copy */
	*(ld->ld_io_fns_ptr) = *iofns;

	ld->ld_extio_size = LBER_X_EXTIO_FNS_SIZE;
	ld->ld_ext_session_arg = defcsip;
	ld->ld_extread_fn = nsldapi_ext_compat_read;
	ld->ld_extwrite_fn = nsldapi_ext_compat_write;
	ld->ld_extpoll_fn = nsldapi_ext_compat_poll;
	ld->ld_extconnect_fn = nsldapi_ext_compat_connect;
	ld->ld_extclose_fn = nsldapi_ext_compat_close;

	return( nsldapi_install_lber_extiofns( ld, ld->ld_sbp ));
}
/*
 * end of compat I/O functions
 ******************************************************************************
 */
