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
 *  macos-ip.c -- Macintosh platform-specific TCP & UDP related code
 */

#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1995 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <Memory.h>
#include "ldap-int.h"


int
nsldapi_connect_to_host( LDAP *ld, Sockbuf *sb, char *host, unsigned long address,
	int port, int async, int secure )
/*
 * if host == NULL, connect using address
 * "address" and "port" must be in network byte order
 * zero is returned upon success, -1 if fatal error, -2 EINPROGRESS
 * if -1 is returned, ld_errno is set
 * async is only used ifndef NO_REFERRALS (non-0 means don't wait for connect)
 * XXX async is not used yet!
 */
{
	void			*tcps;
	short 			i;
	int				err;
    InetHostInfo	hi;

	LDAPDebug( LDAP_DEBUG_TRACE, "connect_to_host: %s:%d\n",
	    ( host == NULL ) ? "(by address)" : host, ntohs( port ), 0 );

	/* Initialize OpenTransport, or find out from the host app whether it is installed */
	(void)tcp_init();
	
	if ( host != NULL && gethostinfobyname( host, &hi ) != noErr ) {
		LDAP_SET_LDERRNO( ld, LDAP_CONNECT_ERROR, NULL, NULL );
		return( -1 );
	}

	if ( ld->ld_socket_fn == NULL ) {
		tcps = tcpopen( NULL, TCP_BUFSIZ );
	} else {
		tcps = ld->ld_socket_fn( AF_INET, SOCK_STREAM, 0 );
	}

	if ( tcps == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
		return( -1 );
	}

	if ( secure && ld->ld_ssl_enable_fn( tcps ) < 0 ) {
		if ( ld->ld_close_fn == NULL ) {
			tcpclose( (tcpstream *)tcps );
		} else {
			ld->ld_close_fn( tcps );
		}
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
		return( -1 );
	}

    for ( i = 0; host == NULL || hi.addrs[ i ] != 0; ++i ) {
    	if ( host != NULL ) {
			SAFEMEMCPY( (char *)&address, (char *)&hi.addrs[ i ], sizeof( long ));
		}


		if ( ld->ld_connect_fn == NULL ) {
			if ( tcpconnect( tcps, address, port ) == 0 ) {
				err = -1;
			} else {
				err = 0;
			}
		} else {
			struct sockaddr_in	sin;
			(void)memset( (char *)&sin, 0, sizeof( struct sockaddr_in ));
			sin.sin_family = AF_INET;
			sin.sin_port = port;
			sin.sin_addr.s_addr = address;

			err = ld->ld_connect_fn( tcps, (struct sockaddr *)&sin,
				sizeof( struct sockaddr_in ));
		}

		if ( err == 0 ) {
			sb->sb_sd = (void *)tcps;
			return( 0 );
		}

		if ( host == NULL ) {	/* using single address -- not hi.addrs array */
			break;
		}
	}
	
	LDAPDebug( LDAP_DEBUG_TRACE, "tcpconnect failed\n", 0, 0, 0 );
	LDAP_SET_LDERRNO( ld, LDAP_CONNECT_ERROR, NULL, NULL );
	LDAP_SET_ERRNO( ld, EHOSTUNREACH );  /* close enough */

	if ( ld->ld_close_fn == NULL ) {
		tcpclose( (tcpstream *)tcps );
	} else {
		ld->ld_close_fn( tcps );
	}
	return( -1 );
}


void
nsldapi_close_connection( LDAP *ld, Sockbuf *sb )
{
	if ( ld->ld_close_fn == NULL ) {
		tcpclose( (tcpstream *)sb->sb_sd );
	} else {
		ld->ld_close_fn( sb->sb_sd );
	}
}


#ifdef KERBEROS
char *
host_connected_to( Sockbuf *sb )
{
	ip_addr addr;
	
    InetHostInfo	hi;

	if ( tcpgetpeername( (tcpstream *)sb->sb_sd, &addr, NULL ) != noErr ) {
		return( NULL );
	}

	if ( gethostinfobyaddr( addr, &hi ) == noErr ) {
		return( strdup( hi.name ));
	}

	return( NULL );
}
#endif /* KERBEROS */


struct tcpstreaminfo {
	struct tcpstream	*tcpsi_stream;
	Boolean				tcpsi_check_read;
	Boolean				tcpsi_is_read_ready;
	Boolean				tcpsi_check_write;
	Boolean				tcpsi_is_write_ready;
};

/* for BSD-compatible I/O functions */
struct stdselectinfo {
	fd_set	ssi_readfds;
	fd_set	ssi_writefds;
	fd_set	ssi_use_readfds;
	fd_set	ssi_use_writefds;
};

struct selectinfo {
	short					si_count;
	struct tcpstreaminfo	*si_streaminfo;
	struct stdselectinfo	si_stdinfo;
};


void
nsldapi_mark_select_write( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;
	struct tcpstreaminfo	*tcpsip;
	short			i;

	LDAPDebug( LDAP_DEBUG_TRACE, "mark_select_write: stream %x\n",
	    (tcpstream *)sb->sb_sd, 0, 0 );

	if (( sip = (struct selectinfo *)ld->ld_selectinfo ) == NULL ) {
		return;
	}

	if ( ld->ld_select_fn != NULL ) {
		if ( !FD_ISSET( (int)sb->sb_sd, &sip->si_stdinfo.ssi_writefds )) {
		    FD_SET( (int)sb->sb_sd, &sip->si_stdinfo.ssi_writefds );
		}
		return;
	}

	for ( i = 0; i < sip->si_count; ++i ) {    /* make sure stream is not already in the list... */
		if ( sip->si_streaminfo[ i ].tcpsi_stream == (tcpstream *)sb->sb_sd) {
			sip->si_streaminfo[ i ].tcpsi_check_write = true;
			sip->si_streaminfo[ i ].tcpsi_is_write_ready = false;
			return;
		}
	}

	/* add a new stream element to our array... */
	if ( sip->si_count <= 0 ) {
		tcpsip = (struct tcpstreaminfo *)malloc( sizeof( struct tcpstreaminfo ));
	} else {
		tcpsip = (struct tcpstreaminfo *)realloc( sip->si_streaminfo,
		    ( sip->si_count + 1 ) * sizeof( struct tcpstreaminfo ));
	}

	if ( tcpsip != NULL ) {
		tcpsip[ sip->si_count ].tcpsi_stream = (tcpstream *)sb->sb_sd;
		tcpsip[ sip->si_count ].tcpsi_check_write = true;
		tcpsip[ sip->si_count ].tcpsi_is_write_ready = false;
		sip->si_streaminfo = tcpsip;
		++sip->si_count;
	}
}


void
nsldapi_mark_select_read( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo		*sip;
	struct tcpstreaminfo	*tcpsip;
	short					i;
	
	LDAPDebug( LDAP_DEBUG_TRACE, "mark_select_read: stream %x\n", (tcpstream *)sb->sb_sd, 0, 0 );

	if (( sip = (struct selectinfo *)ld->ld_selectinfo ) == NULL ) {
		return;
	}

	if ( ld->ld_select_fn != NULL ) {		
		if ( !FD_ISSET( (int)sb->sb_sd, &sip->si_stdinfo.ssi_readfds )) {
			FD_SET( (int)sb->sb_sd, &sip->si_stdinfo.ssi_readfds );
		}
		return;
	}

	for ( i = 0; i < sip->si_count; ++i ) {	/* make sure stream is not already in the list... */
		if ( sip->si_streaminfo[ i ].tcpsi_stream == (tcpstream *)sb->sb_sd ) {
			sip->si_streaminfo[ i ].tcpsi_check_read = true;
			sip->si_streaminfo[ i ].tcpsi_is_read_ready = false;
            sip->si_streaminfo[ i ].tcpsi_check_write = true;
            sip->si_streaminfo[ i ].tcpsi_is_write_ready = false;
			return;
		}
	}

	/* add a new stream element to our array... */
	if ( sip->si_count <= 0 ) {
		tcpsip = (struct tcpstreaminfo *)malloc( sizeof( struct tcpstreaminfo ));
	} else {
		tcpsip = (struct tcpstreaminfo *)realloc( sip->si_streaminfo,
				( sip->si_count + 1 ) * sizeof( struct tcpstreaminfo ));
	}
	
	if ( tcpsip != NULL ) {
		tcpsip[ sip->si_count ].tcpsi_stream = (tcpstream *)sb->sb_sd;
		tcpsip[ sip->si_count ].tcpsi_check_read = true;
		tcpsip[ sip->si_count ].tcpsi_is_read_ready = false;
		tcpsip[ sip->si_count ].tcpsi_check_write = true;
		tcpsip[ sip->si_count ].tcpsi_is_write_ready = false;
		sip->si_streaminfo = tcpsip;
		++sip->si_count;
	}
}


void
nsldapi_mark_select_clear( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;
	short				i;

	LDAPDebug( LDAP_DEBUG_TRACE, "mark_select_clear: stream %x\n", (tcpstream *)sb->sb_sd, 0, 0 );

	if (( sip = (struct selectinfo *)ld->ld_selectinfo ) == NULL ) {
		return;
	}

	if ( ld->ld_select_fn != NULL ) {
		FD_CLR( (int)sb->sb_sd, &sip->si_stdinfo.ssi_writefds );
		FD_CLR( (int)sb->sb_sd, &sip->si_stdinfo.ssi_readfds );

	} else if ( sip->si_count > 0 && sip->si_streaminfo != NULL ) {
		for ( i = 0; i < sip->si_count; ++i ) {
			if ( sip->si_streaminfo[ i ].tcpsi_stream == (tcpstream *)sb->sb_sd ) {
				break;
			}
		}
		if ( i < sip->si_count ) {
			--sip->si_count;
			for ( ; i < sip->si_count; ++i ) {
				sip->si_streaminfo[ i ] = sip->si_streaminfo[ i + 1 ];
			}
			/* we don't bother to use realloc to make the si_streaminfo array smaller */
		}
	}
}


int
nsldapi_is_write_ready( LDAP *ld, Sockbuf *sb )
{
    struct selectinfo    *sip;
    short                i;
    
    if (( sip = (struct selectinfo *)ld->ld_selectinfo ) == NULL ) {
        return( 0 );    /* punt */
    }

    if ( ld->ld_select_fn != NULL ) {
        return( FD_ISSET( (int)sb->sb_sd, &sip->si_stdinfo.ssi_use_writefds));
    }

    if ( sip->si_count > 0 && sip->si_streaminfo != NULL ) {
        for ( i = 0; i < sip->si_count; ++i ) {
            if ( sip->si_streaminfo[ i ].tcpsi_stream == (tcpstream *)sb->sb_sd ) {
#ifdef LDAP_DEBUG
                if ( sip->si_streaminfo[ i ].tcpsi_is_write_ready ) {
                    LDAPDebug( LDAP_DEBUG_TRACE, "is_write_ready: stream %x READY\n",
                            (tcpstream *)sb->sb_sd, 0, 0 );
                } else {
                    LDAPDebug( LDAP_DEBUG_TRACE, "is_write_ready: stream %x Not Ready\n",
                            (tcpstream *)sb->sb_sd, 0, 0 );
                }
#endif /* LDAP_DEBUG */
                return( sip->si_streaminfo[ i ].tcpsi_is_write_ready ? 1 : 0 );
            }
        }
    }

    LDAPDebug( LDAP_DEBUG_TRACE, "is_write_ready: stream %x: NOT FOUND\n", (tcpstream *)sb->sb_sd, 0, 0 );
    return( 0 );
}


int
nsldapi_is_read_ready( LDAP *ld, Sockbuf *sb )
{
	struct selectinfo	*sip;
	short				i;
	
	if (( sip = (struct selectinfo *)ld->ld_selectinfo ) == NULL ) {
		return( 0 );	/* punt */
	}

	if ( ld->ld_select_fn != NULL ) {
		return( FD_ISSET( (int)sb->sb_sd, &sip->si_stdinfo.ssi_use_readfds ));
	}

	if ( sip->si_count > 0 && sip->si_streaminfo != NULL ) {
		for ( i = 0; i < sip->si_count; ++i ) {
			if ( sip->si_streaminfo[ i ].tcpsi_stream == (tcpstream *)sb->sb_sd ) {
#ifdef LDAP_DEBUG
				if ( sip->si_streaminfo[ i ].tcpsi_is_read_ready ) {
					LDAPDebug( LDAP_DEBUG_TRACE, "is_read_ready: stream %x READY\n",
							(tcpstream *)sb->sb_sd, 0, 0 );
				} else {
					LDAPDebug( LDAP_DEBUG_TRACE, "is_read_ready: stream %x Not Ready\n",
							(tcpstream *)sb->sb_sd, 0, 0 );
				}
#endif /* LDAP_DEBUG */
				return( sip->si_streaminfo[ i ].tcpsi_is_read_ready ? 1 : 0 );
			}
		}
	}

	LDAPDebug( LDAP_DEBUG_TRACE, "is_read_ready: stream %x: NOT FOUND\n", (tcpstream *)sb->sb_sd, 0, 0 );
	return( 0 );
}


void *
nsldapi_new_select_info()
{
	struct selectinfo	*sip;
		
	if (( sip = (struct selectinfo *)calloc( 1, sizeof( struct selectinfo ))) != NULL ) {
		FD_ZERO( &sip->si_stdinfo.ssi_readfds );
		FD_ZERO( &sip->si_stdinfo.ssi_writefds );
	}

	return( (void *)sip );
}


void
nsldapi_free_select_info( void *sip )
{
	if ( sip != NULL ) {
		free( sip );
	}
}


int
nsldapi_do_ldap_select( LDAP *ld, struct timeval *timeout )
{
	struct selectinfo	*sip;
	Boolean				ready, gotselecterr;
	unsigned long		ticks, endticks;
	short				i, err;
	static int			tblsize = 0;
	EventRecord			dummyEvent;

	LDAPDebug( LDAP_DEBUG_TRACE, "do_ldap_select\n", 0, 0, 0 );

	if (( sip = (struct selectinfo *)ld->ld_selectinfo ) == NULL ) {
		return( -1 );
	}

	if ( ld->ld_select_fn != NULL ) {
		if ( tblsize == 0 ) {
			tblsize = FD_SETSIZE - 1;
		}

		sip->si_stdinfo.ssi_use_readfds = sip->si_stdinfo.ssi_readfds;
		sip->si_stdinfo.ssi_use_writefds = sip->si_stdinfo.ssi_writefds;

		return( ld->ld_select_fn( tblsize, &sip->si_stdinfo.ssi_use_readfds,
		    &sip->si_stdinfo.ssi_use_writefds, NULL, timeout ));
	}

	if ( sip->si_count == 0 ) {
		return( 1 );
	}

	if ( timeout != NULL ) {
		endticks = 60 * timeout->tv_sec + ( 60 * timeout->tv_usec ) / 1000000 + TickCount();
	}

	for ( i = 0; i < sip->si_count; ++i ) {
		if ( sip->si_streaminfo[ i ].tcpsi_check_read ) {
			sip->si_streaminfo[ i ].tcpsi_is_read_ready = false;
		}
        if ( sip->si_streaminfo[ i ].tcpsi_check_write ) {
            sip->si_streaminfo[ i ].tcpsi_is_write_ready = false;
        }
	}

	ready = gotselecterr = false;
	do {
		for ( i = 0; i < sip->si_count; ++i ) {
			if ( sip->si_streaminfo[ i ].tcpsi_check_read && !sip->si_streaminfo[ i ].tcpsi_is_read_ready ) {
				if (( err = tcpreadready( sip->si_streaminfo[ i ].tcpsi_stream )) > 0 ) {
					sip->si_streaminfo[ i ].tcpsi_is_read_ready = ready = true;
                } else if ( err < 0 ) {
                    gotselecterr = true;
                }
            }
			if ( sip->si_streaminfo[ i ].tcpsi_check_write && !sip->si_streaminfo[ i ].tcpsi_is_write_ready ) {
					if (( err = tcpwriteready( sip->si_streaminfo[ i ].tcpsi_stream )) > 0 ) {
							sip->si_streaminfo[ i ].tcpsi_is_write_ready = ready = true;
					} else if ( err < 0 ) {
							gotselecterr = true;
					}
			}
		}

		if ( !ready && !gotselecterr ) {
			(void)WaitNextEvent(nullEvent, &dummyEvent, 1, NULL);
			ticks = TickCount();
		}
	} while ( !ready && !gotselecterr && ( timeout == NULL || ticks < endticks ));

	LDAPDebug( LDAP_DEBUG_TRACE, "do_ldap_select returns %d\n", ready ? 1 : ( gotselecterr ? -1 : 0 ), 0, 0 );
	return( ready ? 1 : ( gotselecterr ? -1 : 0 ));
}
