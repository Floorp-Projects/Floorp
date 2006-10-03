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
 * The Original Code is Sun LDAP C SDK.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 *
 * Portions created by Sun Microsystems, Inc are Copyright (C) 2005
 * Sun Microsystems, Inc. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifdef LDAP_SASLIO_HOOKS
#include <assert.h>
#include "ldap-int.h"
#include "../liblber/lber-int.h"
#include <sasl.h>
/* Should be pulled in from lber-int.h */
#define READBUFSIZ	8192

#define SEARCH_TIMEOUT_SECS	120
#define NSLDAPI_SM_BUF	128

/*
 * Data structures:
 */

/* data structure that populates the I/O callback socket-specific arg. */
typedef struct lextiof_socket_private {
	struct ldap_x_ext_io_fns sess_io_fns; /* the saved layered ld fns from the layer we are "pushing" */
	struct lber_x_ext_io_fns sock_io_fns; /* the saved layered ber fns from the layer we are "pushing" */
	sasl_conn_t     *sasl_ctx;     /* the sasl context - pointer to the one from the connection */
	char            *sb_sasl_ibuf; /* sasl decrypted input buffer */
	char            *sb_sasl_iptr; /* current location in buffer */
	int             sb_sasl_bfsz;  /* Alloc'd size of input buffer */
	int             sb_sasl_ilen;  /* remaining length to process */
	LDAP            *ld;           /* used to set errno */
	Sockbuf         *sb;           /* pointer to our associated sockbuf */
} SASLIOSocketArg;

static void
destroy_SASLIOSocketArg(SASLIOSocketArg** sockarg)
{
	if (sockarg && *sockarg) {
		NSLDAPI_FREE((*sockarg)->sb_sasl_ibuf);
		NSLDAPI_FREE((*sockarg));
		*sockarg = NULL;
	}
}

static SASLIOSocketArg*
new_SASLIOSocketArg(sasl_conn_t *ctx, int bufsiz, LDAP *ld, Sockbuf *sb)
{
	SASLIOSocketArg *sockarg = NULL;

	if (bufsiz <= 0) {
		return sockarg;
	}

	sockarg = (SASLIOSocketArg*)NSLDAPI_CALLOC(1, sizeof(SASLIOSocketArg));
	if (sockarg) {
		sockarg->sasl_ctx = ctx;
		sockarg->sb_sasl_ibuf = NSLDAPI_MALLOC(bufsiz);
		if (!sockarg->sb_sasl_ibuf) {
			destroy_SASLIOSocketArg(&sockarg);
			return sockarg;
		}
		sockarg->sb_sasl_iptr = NULL;
		sockarg->sb_sasl_bfsz = bufsiz;
		sockarg->sb_sasl_ilen = 0;
		sockarg->ld           = ld;
		sockarg->sb           = sb;
	}

	return sockarg;
}

/*
 * SASL Dependent routines
 *
 * SASL security and integrity options are supported through the
 * use of the extended I/O functionality.  Because the extended
 * I/O functions may already be in use prior to enabling encryption,
 * when SASL encryption si enabled, these routine interpose themselves
 * over the exitng extended I/O routines and add an additional level
 * of indirection.
 *  IE: Before SASL:  client->libldap->lber->extio
 *      After  SASL:  client->libldap->lber->saslio->extio
 * Any extio function are stilled used for the raw i/O [IE prldap]
 * but SASL will decrypt before passing to lber.
 * SASL cannot decrypt a stream so full packaets must be read
 * before proceeding.
 */

/*
 * Get the 4 octet header [size] for a sasl encrypted buffer.
 * See RFC222 [section 3].
 */
static int
nsldapi_sasl_pktlen( char *buf, int maxbufsize )
{
        int     size;

        size = ntohl(*(uint32_t *)buf);

        if ( size < 0 || size > maxbufsize ) {
                return (-1 );
        }

        return( size + 4 ); /* include the first 4 bytes */
}

/*
 * SASL encryption routines
 */

static int
nsldapi_sasl_read( int s, void *buf, int  len,
	struct lextiof_socket_private *arg)
{
	LDAP		*ld;
	const char	*dbuf;
	char		*cp;
	int		ret;
	unsigned	dlen, blen;
   
	ld = (LDAP *)arg->ld;

	/* Is there anything left in the existing buffer? */
	if ((ret = arg->sb_sasl_ilen) > 0) {
		ret = (ret > len ? len : ret);
		SAFEMEMCPY( buf, arg->sb_sasl_iptr, ret );
		if (ret == arg->sb_sasl_ilen) {
			arg->sb_sasl_ilen = 0;
			arg->sb_sasl_iptr = NULL;
		} else {
			arg->sb_sasl_ilen -= ret;
			arg->sb_sasl_iptr += ret;
		}
		return( ret );
	}

	/* buffer is empty - fill it */
	cp = arg->sb_sasl_ibuf;
	dlen = 0;
	
	/* Read the length of the packet */
	while ( dlen < 4 ) {
		if (arg->sock_io_fns.lbextiofn_read != NULL) {
			ret = arg->sock_io_fns.lbextiofn_read(
				s, cp, 4 - dlen,
				arg->sock_io_fns.lbextiofn_socket_arg);
		} else {
			ret = read( s, cp, 4 - dlen );
		}
#ifdef EINTR
		if ( ( ret < 0 ) && ( LDAP_GET_ERRNO(ld) == EINTR ) )
			continue;
#endif
		if ( ret <= 0 )
			return( ret );

		cp += ret;
		dlen += ret;
	}

	blen = 4;

	ret = nsldapi_sasl_pktlen( arg->sb_sasl_ibuf, arg->sb_sasl_bfsz );
	if (ret < 0) {
		LDAP_SET_ERRNO(ld, EIO);
		return( -1 );
	}
	dlen = ret - dlen;

	/* read the rest of the encrypted packet */
	while ( dlen > 0 ) {
		if (arg->sock_io_fns.lbextiofn_read != NULL) {
			ret = arg->sock_io_fns.lbextiofn_read(
				s, cp, dlen,
				arg->sock_io_fns.lbextiofn_socket_arg);
		} else {
			ret = read( s, cp, dlen );
		}

#ifdef EINTR
		if ( ( ret < 0 ) && ( LDAP_GET_ERRNO(ld) == EINTR ) )
			continue;
#endif
		if ( ret <= 0 )
			return( ret );

		cp += ret;
		blen += ret;
		dlen -= ret;
   	}

	/* Decode the packet */
	ret = sasl_decode( arg->sasl_ctx,
			   arg->sb_sasl_ibuf, blen,
			   &dbuf, &dlen);
	if ( ret != SASL_OK ) {
		/* sb_sasl_read: failed to decode packet, drop it, error */
		arg->sb_sasl_iptr = NULL;
		arg->sb_sasl_ilen = 0;
		LDAP_SET_ERRNO(ld, EIO);
		return( -1 );
	}
	
	/* copy decrypted packet to the input buffer */
	SAFEMEMCPY( arg->sb_sasl_ibuf, dbuf, dlen );
	arg->sb_sasl_iptr = arg->sb_sasl_ibuf;
	arg->sb_sasl_ilen = dlen;

	ret = (dlen > (unsigned) len ? len : dlen);
	SAFEMEMCPY( buf, arg->sb_sasl_iptr, ret );
	if (ret == arg->sb_sasl_ilen) {
		arg->sb_sasl_ilen = 0;
		arg->sb_sasl_iptr = NULL;
	} else {
		arg->sb_sasl_ilen -= ret;
		arg->sb_sasl_iptr += ret;
	}
	return( ret );
}

static int
nsldapi_sasl_write( int s, const void *buf, int  len,
	struct lextiof_socket_private *arg)
{
	int			ret = 0;
	const char	*obuf, *optr, *cbuf = (const char *)buf;
	unsigned	olen, clen, tlen = 0;
	unsigned	*maxbuf; 
	
	ret = sasl_getprop(arg->sasl_ctx, SASL_MAXOUTBUF,
					   (const void **)&maxbuf);
	if ( ret != SASL_OK ) {
		/* just a sanity check, should never happen */
		return( -1 );
	}
	
	while (len > 0) {
		clen = (len > *maxbuf) ? *maxbuf : len;
		/* encode the next packet. */
		ret = sasl_encode( arg->sasl_ctx, cbuf, clen, &obuf, &olen);
		if ( ret != SASL_OK ) {
			/* XXX Log error? "sb_sasl_write: failed to encode packet..." */
			return( -1 );
		}
		/* Write everything now, buffer is only good until next sasl_encode */
		optr = obuf;
		while (olen > 0) {
			if (arg->sock_io_fns.lbextiofn_write != NULL) {
				ret = arg->sock_io_fns.lbextiofn_write(
					s, optr, olen,
					arg->sock_io_fns.lbextiofn_socket_arg);
			} else {
				ret = write( s, optr, olen);
			}
			if ( ret < 0 )
				return( ret );
			optr += ret;
			olen -= ret;
		}
		len -= clen;
		cbuf += clen;
		tlen += clen;
	}
	return( tlen );
}

/*
 * What's all this then?
 * First, take a look at os-ip.c:nsldapi_add_to_cb_pollfds().  When a new descriptor is
 * added to the pollfds array, the lpoll_socketarg field is initialized to the value from
 * the socketarg field - sb->sb_ext_io_fns.lbextiofn_socket_arg.  In our case, since we
 * override this with our sasl data (see below nsldapi_sasl_install), we need to restore
 * the previous value so that the layer below us (i.e. prldap) can use the lpoll_socketarg
 * which it sets.
 * So how do which know which fds[i] is a "sasl" fd?
 * We initialize the lextiof_session_private *arg (see nsldapi_sasl_install) to point to
 * the socket_private data in sb->sb_ext_io_fns.lbextiofn_socket_arg for "sasl" sockets,
 * which is then used to initialize lpoll_socketarg (see above).
 * So, if the arg which gets passed into nsldapi_sasl_poll is the same as the
 * fds[i].lpoll_socketarg, we know it is a "sasl" socket and we need to "pop" the sasl
 * layer.  We do this by replacing lpoll_socketarg with the one we saved when we "pushed"
 * the sasl layer.
 * So why the loop to restore the sasl lpoll_socketarg?
 * The lower layer only uses lpoll_socketarg during poll().  See ldappr-io.c:prldap_poll()
 * for more information about how that works.  However, after the polling is done, there
 * is some special magic in os-ip.c in the functions nsldapi_add_to_cb_pollfds(),
 * nsldapi_clear_from_cb_pollfds(), and nsldapi_find_in_cb_pollfds() to find the correct
 * Sockbuf to operate on.  This is the macro NSLDAPI_CB_POLL_MATCH().  For the extended
 * io function callbacks to work correctly, it is not sufficient to say that the file
 * descriptor in the Sockbuf matches the one that poll says has activity - we also need
 * to match the lpoll_socketarg with the sb->sb_ext_io_fns.lbextiofn_socket_arg to make
 * sure this really is the Sockbuf we want to use.  So we have to restore the
 * lpoll_socketarg with the original one.
 * Why have origarg and staticorigarg?
 * To avoid malloc.  The sizeof staticorigarg should be large enough to accomodate almost
 * all clients without incurring too much additional overhead.  However, if we need more
 * room, origarg will grow to nfds.  If this proves to be inadequate, the size of the
 * staticorigarg is a good candidate for a #define set by configure.
 */
static int
nsldapi_sasl_poll(
	LDAP_X_PollFD fds[], int nfds, int timeout,
	struct lextiof_session_private *arg ) 
{
	LDAP_X_EXTIOF_POLL_CALLBACK *origpoll; /* poll fn from the pushed layer */
	struct lextiof_session_private *origsess = NULL; /* session arg from the pushed layer */
	SASLIOSocketArg **origarg = NULL; /* list of saved original socket args */
	SASLIOSocketArg *staticorigarg[1024]; /* default list to avoid malloc */
	int origargsize = sizeof(staticorigarg)/sizeof(staticorigarg[0]);
	int rc = -1; /* the return code - -1 means failure */

	if (arg == NULL) { /* should not happen */
		return( rc );
	}

	origarg = staticorigarg;
	/* if the static array is not large enough, alloc a dynamic one */
	if (origargsize < nfds) {
		origarg = (SASLIOSocketArg **)NSLDAPI_MALLOC(nfds*sizeof(SASLIOSocketArg *));
	}

	if (fds && nfds > 0) {
		int i;
		for(i = 0; i < nfds; i++) {
			/* save the original socket arg */
			origarg[i] = fds[i].lpoll_socketarg;
			if (arg == (struct lextiof_session_private *)fds[i].lpoll_socketarg) {
				/* lpoll_socketarg is a sasl socket arg - we need to replace it
				   with the one from the layer we pushed (i.e. prldap) */
				SASLIOSocketArg *sockarg = (SASLIOSocketArg *)fds[i].lpoll_socketarg;
				/* reset to pushed layer's socket arg */
				fds[i].lpoll_socketarg = sockarg->sock_io_fns.lbextiofn_socket_arg;
				/* grab the pushed layers' poll fn and its session arg */
				if (!origsess) {
					origpoll = sockarg->sess_io_fns.lextiof_poll;
					origsess = sockarg->sess_io_fns.lextiof_session_arg;
				}
			}
		}
	}

	if (origsess == NULL) { /* should not happen */
		goto done;
	}

	/* call the "real" poll function */
	rc = origpoll( fds, nfds, timeout, origsess );

	/* reset the lpoll_socketarg values to their original values because
	   they must match what's in sb->iofns->lbextiofn_socket_arg in order
	   for NSLDAPI_CB_POLL_MATCH to work - see os-ip.c */
	if (fds && nfds > 0) {
		int i;
		for(i = 0; i < nfds; i++) {
			if ((SASLIOSocketArg *)arg == origarg[i]) {
				fds[i].lpoll_socketarg = origarg[i];
			}
		}
	}

done:
	/* if we had to use a dynamic array, free it */
	if (origarg != staticorigarg) {
		NSLDAPI_FREE(origarg);
	}

	return rc;
}

int
nsldapi_sasl_open( LDAP *ld, LDAPConn *lconn, sasl_conn_t **ctx, sasl_ssf_t ssf )
{
        int saslrc;
        char *host = NULL;

        if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
                LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
                return( LDAP_LOCAL_ERROR );
        }

        if ( lconn == NULL ) {
                if ( ld->ld_defconn == NULL ||
                     ld->ld_defconn->lconn_status != LDAP_CONNST_CONNECTED) {
                        int rc = nsldapi_open_ldap_defconn( ld );
                        if( rc < 0 )  {
                                return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
                        }
                }
                lconn = ld->ld_defconn;
        }

        /* need to clear out the old context for this connection, if any */
        /* client may have re-bind-ed this connection without closing first */
        if (lconn->lconn_sasl_ctx) {
            sasl_dispose(&lconn->lconn_sasl_ctx);
            lconn->lconn_sasl_ctx = NULL;
        }

        if ( 0 != ldap_get_option( ld, LDAP_OPT_HOST_NAME, &host ) ) {
                LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
                return( LDAP_LOCAL_ERROR );
        }

        saslrc = sasl_client_new( "ldap", host,
                NULL, NULL, /* iplocalport, ipremoteport - use defaults */
                NULL, 0, ctx );
        ldap_memfree(host);

        if ( (saslrc != SASL_OK) || (!*ctx) ) {
                return( nsldapi_sasl_cvterrno( ld, saslrc, NULL ) );
        }

        if( ssf ) {
                sasl_ssf_t extprops;
                memset(&extprops, 0L, sizeof(extprops));
                extprops = ssf;

                (void) sasl_setprop( *ctx, SASL_SSF_EXTERNAL,
                        (void *) &extprops );
        }

        /* (re)set security properties */
        sasl_setprop( *ctx, SASL_SEC_PROPS, &ld->ld_sasl_secprops );

        /* set the connection context */
        lconn->lconn_sasl_ctx = *ctx;

        return( LDAP_SUCCESS );
}

static int
nsldapi_sasl_close( struct lextiof_socket_private *arg )
{
	/* undo function pointer interposing */
	ldap_set_option( arg->ld, LDAP_X_OPT_EXTIO_FN_PTRS, &arg->sess_io_fns );
	/* have to do this separately to make sure the socketarg is set correctly */
	ber_sockbuf_set_option( arg->sb,
							LBER_SOCKBUF_OPT_EXT_IO_FNS,
							(void *)&arg->sock_io_fns );

	destroy_SASLIOSocketArg(&arg);
	return( LDAP_SUCCESS );
}

static int
nsldapi_sasl_close_socket(int s, struct lextiof_socket_private *arg ) 
{
	LDAP_X_EXTIOF_CLOSE_CALLBACK *origclose;
	struct lextiof_socket_private *origsock;

	if (arg == NULL) {
		return( -1 );
	}

	origclose = arg->sess_io_fns.lextiof_close;
	origsock = arg->sock_io_fns.lbextiofn_socket_arg;

	/* undo SASL */
	nsldapi_sasl_close( arg );
	arg = NULL;
	/* arg is destroyed at this point - do not use it */

	return ( origclose( s, origsock ) );
}

/*
 * install encryption routines if security has been negotiated
 */
int
nsldapi_sasl_install( LDAP *ld, LDAPConn *lconn )
{
        struct lber_x_ext_io_fns        fns;
        struct ldap_x_ext_io_fns        iofns;
        sasl_security_properties_t      *secprops;
        int     rc, value;
        int     bufsiz;
        Sockbuf *sb = NULL;
        sasl_conn_t *ctx = NULL;
        SASLIOSocketArg *sockarg = NULL;

        if ( lconn == NULL ) {
                lconn = ld->ld_defconn;
                if ( lconn == NULL ) {
                        return( LDAP_LOCAL_ERROR );
                }
        }
        if ( (sb = lconn->lconn_sb) == NULL ) {
                return( LDAP_LOCAL_ERROR );
        }
        rc = ber_sockbuf_get_option( sb,
                        LBER_SOCKBUF_OPT_TO_FILE_ONLY,
                        (void *) &value);
        if (rc != 0 || value != 0) {
                return( LDAP_LOCAL_ERROR );
        }

        /* the sasl context in the lconn must have been set prior to this */
        ctx = lconn->lconn_sasl_ctx;
        rc = sasl_getprop( ctx, SASL_SEC_PROPS,
                           (const void **)&secprops );
        if (rc != SASL_OK)
                return( LDAP_LOCAL_ERROR );
        bufsiz = secprops->maxbufsize;
        if (bufsiz <= 0) {
                return( LDAP_LOCAL_ERROR );
        }

        /* create our socket specific context */
        sockarg = new_SASLIOSocketArg(ctx, bufsiz, ld, sb);
        if (!sockarg) {
                return( LDAP_LOCAL_ERROR );
        }

        /* save a copy of the existing io fns and the session arg */
        memset( &sockarg->sess_io_fns, 0, LDAP_X_EXTIO_FNS_SIZE );
        sockarg->sess_io_fns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
        rc = ldap_get_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS,
                              &sockarg->sess_io_fns );
        if (rc != 0) {
                destroy_SASLIOSocketArg(&sockarg);
                return( LDAP_LOCAL_ERROR );
        }

        /* save a copy of the existing ber io fns and the socket arg */
        memset( &sockarg->sock_io_fns, 0, LBER_X_EXTIO_FNS_SIZE );
        sockarg->sock_io_fns.lbextiofn_size = LBER_X_EXTIO_FNS_SIZE;
        rc = ber_sockbuf_get_option( sb,
                        LBER_SOCKBUF_OPT_EXT_IO_FNS,
                        (void *)&sockarg->sock_io_fns);
        if (rc != 0) {
                destroy_SASLIOSocketArg(&sockarg);
                return( LDAP_LOCAL_ERROR );
        }

        /* Set new values for the ext io funcs if there are any -
           when using the native io fns (as opposed to prldap) there
           won't be any */
        if (  sockarg->sess_io_fns.lextiof_read != NULL ||
              sockarg->sess_io_fns.lextiof_write != NULL ||
              sockarg->sess_io_fns.lextiof_poll != NULL ||
              sockarg->sess_io_fns.lextiof_connect != NULL ||
              sockarg->sess_io_fns.lextiof_close != NULL ) {
                memset( &iofns, 0, sizeof(iofns));
				/* first, copy struct - sets defaults */
				iofns = sockarg->sess_io_fns;
				/* next, just reset those functions we want to override */
                iofns.lextiof_read = nsldapi_sasl_read;
                iofns.lextiof_write = nsldapi_sasl_write;
                iofns.lextiof_poll = nsldapi_sasl_poll;
                iofns.lextiof_close = nsldapi_sasl_close_socket;
                iofns.lextiof_session_arg = sockarg; /* this is key in nsldapi_sasl_poll */
                rc = ldap_set_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS,
                              &iofns );
                if (rc != 0) {
                        /* frees everything and resets fns above */
                        nsldapi_sasl_close(sockarg);
                        return( LDAP_LOCAL_ERROR );
                }
        }

        /* set the new ber io funcs and socket arg */
        (void) memset( &fns, 0, LBER_X_EXTIO_FNS_SIZE);
        fns.lbextiofn_size = LBER_X_EXTIO_FNS_SIZE;
        fns.lbextiofn_read = nsldapi_sasl_read;
        fns.lbextiofn_write = nsldapi_sasl_write;
        fns.lbextiofn_socket_arg = sockarg;
        rc = ber_sockbuf_set_option( sb,
                        LBER_SOCKBUF_OPT_EXT_IO_FNS,
                        (void *)&fns);
        if (rc != 0) {
                /* frees everything and resets fns above */
                nsldapi_sasl_close(sockarg);
                return( LDAP_LOCAL_ERROR );
        }

        return( LDAP_SUCCESS );
}

#endif
