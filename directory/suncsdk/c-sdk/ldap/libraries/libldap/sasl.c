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
#ifndef DJL_BAD_BUILD
/* Should be pulled in from lber-int.h */
#define READBUFSIZ	8192
#endif

#define SEARCH_TIMEOUT_SECS	120
#define NSLDAPI_SM_BUF	128

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

static int nsldapi_sasl_fail()
{
	return( SASL_FAIL );
}

/*
 * Global SASL Init data
 */

static sasl_callback_t client_callbacks[] = {
	{ SASL_CB_GETOPT, nsldapi_sasl_fail, NULL },
	{ SASL_CB_GETREALM, NULL, NULL },
	{ SASL_CB_USER, NULL, NULL },
	{ SASL_CB_AUTHNAME, NULL, NULL },
	{ SASL_CB_PASS, NULL, NULL },
	{ SASL_CB_ECHOPROMPT, NULL, NULL },
	{ SASL_CB_NOECHOPROMPT, NULL, NULL },
	{ SASL_CB_LIST_END, NULL, NULL }
};
static int nsldapi_sasl_inited = 0;

int nsldapi_sasl_is_inited()
{
	return( nsldapi_sasl_inited );
}

int nsldapi_sasl_init( void )
{
	if ( nsldapi_sasl_inited ) {
		return( 0 );
	}

	sasl_set_alloc(
		(sasl_malloc_t *)ldap_x_malloc,
		(sasl_calloc_t *)ldap_x_calloc,
		(sasl_realloc_t *)ldap_x_realloc,
		(sasl_free_t *)ldap_x_free );

	if ( sasl_client_init( client_callbacks ) == SASL_OK ) {
		nsldapi_sasl_inited = 1;
		return( 0 );
	}

	return( -1 );
}

/*
 * SASL encryption routines
 */

/*
 * Get the 4 octet header [size] for a sasl encrypted buffer.
 * See RFC222 [section 3].
 */

static int
nsldapi_sasl_pktlen( char *buf, int maxbufsize )
{
	int	size;

#ifndef DJL_BAD_BUILD
	size = ntohl(*(long *)buf);
#else
	size = LBER_NTOHL(*(long *)buf);
#endif
   
	if ( size < 0 || size > maxbufsize ) {
		return (-1 );
	}

	return( size + 4 ); /* include the first 4 bytes */
}

static int
nsldapi_sasl_read( int s, void *buf, int  len,
	struct lextiof_socket_private *arg)
{
	Sockbuf		*sb = (Sockbuf *)arg;
	LDAP		*ld;
	const char	*dbuf;
	char		*cp;
	int		ret;
	unsigned	dlen, blen;
   
	ld = (LDAP *)sb->sb_sasl_prld;

	/* Is there anything left in the existing buffer? */
	if ((ret = sb->sb_sasl_ilen) > 0) {
		ret = (ret > len ? len : ret);
		SAFEMEMCPY( buf, sb->sb_sasl_iptr, ret );
		if (ret == sb->sb_sasl_ilen) {
			sb->sb_sasl_ilen = 0;
			sb->sb_sasl_iptr = NULL;
		} else {
			sb->sb_sasl_ilen -= ret;
			sb->sb_sasl_iptr += ret;
		}
		return( ret );
	}

	/* buffer is empty - fill it */
	cp = sb->sb_sasl_ibuf;
	dlen = 0;
	
	/* Read the length of the packet */
	while ( dlen < 4 ) {
		if (sb->sb_sasl_fns.lbextiofn_read != NULL) {
			ret = sb->sb_sasl_fns.lbextiofn_read(
				s, cp, 4 - dlen,
				sb->sb_sasl_fns.lbextiofn_socket_arg);
		} else {
			ret = read( sb->sb_sd, cp, 4 - dlen );
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

	ret = nsldapi_sasl_pktlen( sb->sb_sasl_ibuf, sb->sb_sasl_bfsz );
	if (ret < 0) {
		LDAP_SET_ERRNO(ld, EIO);
		return( -1 );
	}
	dlen = ret - dlen;

	/* read the rest of the encrypted packet */
	while ( dlen > 0 ) {
		if (sb->sb_sasl_fns.lbextiofn_read != NULL) {
			ret = sb->sb_sasl_fns.lbextiofn_read(
				s, cp, dlen,
				sb->sb_sasl_fns.lbextiofn_socket_arg);
		} else {
			ret = read( sb->sb_sd, cp, dlen );
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
	ret = sasl_decode( sb->sb_sasl_ctx,
			   sb->sb_sasl_ibuf, blen,
			   &dbuf, &dlen);
	if ( ret != SASL_OK ) {
		/* sb_sasl_read: failed to decode packet, drop it, error */
		sb->sb_sasl_iptr = NULL;
		sb->sb_sasl_ilen = 0;
		LDAP_SET_ERRNO(ld, EIO);
		return( -1 );
	}
	
	/* copy decrypted packet to the input buffer */
	SAFEMEMCPY( sb->sb_sasl_ibuf, dbuf, dlen );
	sb->sb_sasl_iptr = sb->sb_sasl_ibuf;
	sb->sb_sasl_ilen = dlen;

	ret = (dlen > (unsigned) len ? len : dlen);
	SAFEMEMCPY( buf, sb->sb_sasl_iptr, ret );
	if (ret == sb->sb_sasl_ilen) {
		sb->sb_sasl_ilen = 0;
		sb->sb_sasl_iptr = NULL;
	} else {
		sb->sb_sasl_ilen -= ret;
		sb->sb_sasl_iptr += ret;
	}
	return( ret );
}

static int
nsldapi_sasl_write( int s, const void *buf, int  len,
	struct lextiof_socket_private *arg)
{
	Sockbuf		*sb = (Sockbuf *)arg;
	int			ret = 0;
	const char	*obuf, *optr, *cbuf = (const char *)buf;
	unsigned	olen, clen, tlen = 0;
	unsigned	*maxbuf; 
	
	ret = sasl_getprop(sb->sb_sasl_ctx, SASL_MAXOUTBUF,
					   (const void **)&maxbuf);
	if ( ret != SASL_OK ) {
		/* just a sanity check, should never happen */
		return( -1 );
	}
	
	while (len > 0) {
		clen = (len > *maxbuf) ? *maxbuf : len;
		/* encode the next packet. */
		ret = sasl_encode( sb->sb_sasl_ctx, cbuf, clen, &obuf, &olen);
		if ( ret != SASL_OK ) {
			/* XXX Log error? "sb_sasl_write: failed to encode packet..." */
			return( -1 );
		}
		/* Write everything now, buffer is only good until next sasl_encode */
		optr = obuf;
		while (olen > 0) {
			if (sb->sb_sasl_fns.lbextiofn_write != NULL) {
				ret = sb->sb_sasl_fns.lbextiofn_write(
					s, optr, olen,
					sb->sb_sasl_fns.lbextiofn_socket_arg);
			} else {
				ret = write( sb->sb_sd, optr, olen);
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

static int
nsldapi_sasl_poll(
	LDAP_X_PollFD fds[], int nfds, int timeout,
	struct lextiof_session_private *arg ) 
{
	Sockbuf		*sb = (Sockbuf *)arg;
	LDAP		*ld;
	int		i;

	if (sb == NULL) {
		return( -1 );
	}
	ld = (LDAP *)sb->sb_sasl_prld;

	if (fds && nfds > 0) {
		for(i = 0; i < nfds; i++) {
			if (fds[i].lpoll_socketarg ==
			     (struct lextiof_socket_private *)sb) {
				fds[i].lpoll_socketarg =
					(struct lextiof_socket_private *)
					sb->sb_sasl_fns.lbextiofn_socket_arg;
			}

		}
	}
	return ( ld->ld_sasl_io_fns.lextiof_poll( fds, nfds, timeout,
		(void *)ld->ld_sasl_io_fns.lextiof_session_arg) );
}

int nsldapi_sasl_close( LDAP *ld, Sockbuf *sb )
{
	sasl_conn_t	*ctx = (sasl_conn_t *)sb->sb_sasl_ctx;
	
	if( ctx != NULL ) {
		sasl_dispose( &ctx );
		sb->sb_sasl_ctx = NULL;
	}
	
	return( LDAP_SUCCESS );
}

static int
nsldapi_sasl_close_socket(int s, struct lextiof_socket_private *arg ) 
{
	Sockbuf		*sb = (Sockbuf *)arg;
	LDAP		*ld;

	if (sb == NULL) {
		return( -1 );
	}
	ld = (LDAP *)sb->sb_sasl_prld;
	/* undo function pointer interposing */
	ldap_set_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS, &ld->ld_sasl_io_fns );
	ber_sockbuf_set_option( sb,
			LBER_SOCKBUF_OPT_EXT_IO_FNS,
			(void *)&sb->sb_sasl_fns);

	/* undo SASL */
	nsldapi_sasl_close( ld, sb );

	return ( ld->ld_sasl_io_fns.lextiof_close( s,
		(struct lextiof_socket_private *)
		sb->sb_sasl_fns.lbextiofn_socket_arg ) );
}

/*
 * install encryption routines if security has been negotiated
 */
static int
nsldapi_sasl_install( LDAP *ld, Sockbuf *sb, void *ctx_arg )
{
	struct lber_x_ext_io_fns	fns;
	struct ldap_x_ext_io_fns	iofns;
	sasl_security_properties_t 	*secprops;
	int	rc, value;
	int	bufsiz;

	rc = ber_sockbuf_get_option( sb,
			LBER_SOCKBUF_OPT_TO_FILE_ONLY,
			(void *) &value);
	if (rc != 0 || value != 0)
		return( LDAP_LOCAL_ERROR );

	/* initialize input buffer - use MAX SIZE to avoid reallocs */
	sb->sb_sasl_ctx = (sasl_conn_t *)ctx_arg;
	rc = sasl_getprop( sb->sb_sasl_ctx, SASL_SEC_PROPS,
			   (const void **)&secprops );
	if (rc != SASL_OK)
		return( LDAP_LOCAL_ERROR );
	bufsiz = secprops->maxbufsize;
	if (bufsiz <= 0) {
		return( LDAP_LOCAL_ERROR );
	}
	if ((sb->sb_sasl_ibuf = NSLDAPI_MALLOC(bufsiz)) == NULL) {
		return( LDAP_LOCAL_ERROR );
	}
	sb->sb_sasl_iptr = NULL;
	sb->sb_sasl_bfsz = bufsiz;
	sb->sb_sasl_ilen = 0;

	/* Reset Session then Socket Args */
	/* Get old values */
	(void) memset( &sb->sb_sasl_fns, 0, LBER_X_EXTIO_FNS_SIZE);
	sb->sb_sasl_fns.lbextiofn_size = LBER_X_EXTIO_FNS_SIZE;
	rc = ber_sockbuf_get_option( sb,
			LBER_SOCKBUF_OPT_EXT_IO_FNS,
			(void *)&sb->sb_sasl_fns);
	memset( &ld->ld_sasl_io_fns, 0, sizeof(iofns));
	ld->ld_sasl_io_fns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
	rc = ldap_get_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS,
			      &ld->ld_sasl_io_fns );
	if (rc != 0 )
		return( LDAP_LOCAL_ERROR );

	/* Set new values */
	if (  ld->ld_sasl_io_fns.lextiof_read != NULL ||
	      ld->ld_sasl_io_fns.lextiof_write != NULL ||
	      ld->ld_sasl_io_fns.lextiof_poll != NULL ||
	      ld->ld_sasl_io_fns.lextiof_connect != NULL ||
	      ld->ld_sasl_io_fns.lextiof_close != NULL ) {
		memset( &iofns, 0, sizeof(iofns));
		iofns.lextiof_size = LDAP_X_EXTIO_FNS_SIZE;
		iofns.lextiof_read = nsldapi_sasl_read;
		iofns.lextiof_write = nsldapi_sasl_write;
		iofns.lextiof_poll = nsldapi_sasl_poll;
		iofns.lextiof_connect = ld->ld_sasl_io_fns.lextiof_connect;
		iofns.lextiof_close = nsldapi_sasl_close_socket;
		iofns.lextiof_newhandle = ld->ld_sasl_io_fns.lextiof_newhandle;
		iofns.lextiof_disposehandle =
				ld->ld_sasl_io_fns.lextiof_disposehandle;
		iofns.lextiof_session_arg =
				(void *) sb;
				/* ld->ld_sasl_io_fns.lextiof_session_arg; */
		rc = ldap_set_option( ld, LDAP_X_OPT_EXTIO_FN_PTRS,
			      &iofns );
		if (rc != 0 )
			return( LDAP_LOCAL_ERROR );
		sb->sb_sasl_prld = (void *)ld;
	}

	(void) memset( &fns, 0, LBER_X_EXTIO_FNS_SIZE);
	fns.lbextiofn_size = LBER_X_EXTIO_FNS_SIZE;
	fns.lbextiofn_read = nsldapi_sasl_read;
	fns.lbextiofn_write = nsldapi_sasl_write;
	fns.lbextiofn_socket_arg =
			(void *) sb;
			/* (void *)sb->sb_sasl_fns.lbextiofn_socket_arg; */
	rc = ber_sockbuf_set_option( sb,
			LBER_SOCKBUF_OPT_EXT_IO_FNS,
			(void *)&fns);
	if (rc != 0)
		return( LDAP_LOCAL_ERROR );

	return( LDAP_SUCCESS );
}

static int
nsldapi_sasl_cvterrno( LDAP *ld, int err )
{
	int rc = LDAP_LOCAL_ERROR;

	switch (err) {
	case SASL_OK:
		rc = LDAP_SUCCESS;
		break;
	case SASL_NOMECH:
		rc = LDAP_AUTH_UNKNOWN;
		break;
	case SASL_BADSERV:
		rc = LDAP_CONNECT_ERROR;
		break;
	case SASL_DISABLED:
	case SASL_ENCRYPT:
	case SASL_EXPIRED:
	case SASL_NOUSERPASS:
	case SASL_NOVERIFY:
	case SASL_PWLOCK:
	case SASL_TOOWEAK:
	case SASL_UNAVAIL:
	case SASL_WEAKPASS:
		rc = LDAP_INAPPROPRIATE_AUTH;
		break;
	case SASL_BADAUTH:
	case SASL_NOAUTHZ:
		rc = LDAP_INVALID_CREDENTIALS;
		break;
	case SASL_NOMEM:
		rc = LDAP_NO_MEMORY;
		break;
	case SASL_NOUSER:
		rc = LDAP_NO_SUCH_OBJECT;
		break;
	case SASL_CONTINUE:
	case SASL_FAIL:
	case SASL_INTERACT:
	default:
		rc = LDAP_LOCAL_ERROR;
		break;
	}

	LDAP_SET_LDERRNO( ld, rc, NULL, NULL );
	return( rc );
}

int
nsldapi_sasl_open( LDAP *ld, Sockbuf *sb, const char * host, sasl_ssf_t ssf )
{
	int saslrc;
	sasl_conn_t *ctx;

	if ( host == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
		return( LDAP_LOCAL_ERROR );
	}

	saslrc = sasl_client_new( "ldap", host,
		NULL, NULL, /* iplocalport, ipremoteport - use defaults */
		NULL, 0, &ctx );

	if ( saslrc != SASL_OK ) {
		return( nsldapi_sasl_cvterrno( ld, saslrc ) );
	}

	sb->sb_sasl_ctx = (void *)ctx;

	if( ssf ) {
		sasl_ssf_t extprops;
		memset(&extprops, 0L, sizeof(extprops));
		extprops = ssf;

		(void) sasl_setprop( ctx, SASL_SSF_EXTERNAL,
			(void *) &extprops );
	}

	return( LDAP_SUCCESS );
}

static int
nsldapi_sasl_do_bind( LDAP *ld, const char *dn,
	const char *mechs, unsigned flags,
	LDAP_SASL_INTERACT_PROC *callback, void *defaults,
	LDAPControl **sctrl, LDAPControl **cctrl )
{
	sasl_interact_t *prompts = NULL;
	sasl_conn_t	*ctx;
	sasl_ssf_t	*ssf = NULL;
	const char	*mech = NULL;
	int		saslrc, rc;
	struct berval	ccred;
	unsigned	credlen;

	if (NSLDAPI_LDAP_VERSION( ld ) < LDAP_VERSION3) {
		LDAP_SET_LDERRNO( ld, LDAP_NOT_SUPPORTED, NULL, NULL );
		return( LDAP_NOT_SUPPORTED );
	}

	if ( ld->ld_defconn == NULL ||
	     ld->ld_defconn->lconn_status != LDAP_CONNST_CONNECTED) {
		rc = nsldapi_open_ldap_defconn( ld );
		if( rc < 0 )  {
			return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
		}
	}   

	/* shouldn't happen */
	if (callback == NULL) {
		return( LDAP_LOCAL_ERROR );
	}

	/* expect context to be initialized when connection is open */
	ctx = (sasl_conn_t *)ld->ld_defconn->lconn_sb->sb_sasl_ctx;

	if( ctx == NULL ) {
		LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR, NULL, NULL );
		return( LDAP_LOCAL_ERROR );
	}

	/* (re)set security properties */
	sasl_setprop( ctx, SASL_SEC_PROPS, &ld->ld_sasl_secprops );

	ccred.bv_val = NULL;
	ccred.bv_len = 0;

	do {
		saslrc = sasl_client_start( ctx,
			mechs,
			&prompts,
			(const char **)&ccred.bv_val,
			&credlen,
			&mech );

		LDAPDebug(LDAP_DEBUG_TRACE, "Starting SASL/%s authentication\n",
			  (mech ? mech : ""), 0, 0 );

		if( saslrc == SASL_INTERACT &&
		    (callback)(ld, flags, defaults, prompts) != LDAP_SUCCESS ) {
			break;
		}
	} while ( saslrc == SASL_INTERACT );

	ccred.bv_len = credlen;

	if ( (saslrc != SASL_OK) && (saslrc != SASL_CONTINUE) ) {
		return( nsldapi_sasl_cvterrno( ld, saslrc ) );
	}

	do {
		struct berval *scred;

		scred = NULL;

		/* notify server of a sasl bind step */
		rc = ldap_sasl_bind_s(ld, dn, mech, &ccred,
				      sctrl, cctrl, &scred);

		if ( ccred.bv_val != NULL ) {
			ccred.bv_val = NULL;
		}

		if ( rc != LDAP_SUCCESS && rc != LDAP_SASL_BIND_IN_PROGRESS ) {
			if( scred && scred->bv_len ) {
				/* and server provided us with data? */
				ber_bvfree( scred );
			}
			return( rc );
		}

		if( rc == LDAP_SUCCESS && saslrc == SASL_OK ) {
			/* we're done, no need to step */
			if( scred && scred->bv_len ) {
				/* but server provided us with data! */
				ber_bvfree( scred );
				LDAP_SET_LDERRNO( ld, LDAP_LOCAL_ERROR,
						  NULL, NULL );
				return( LDAP_LOCAL_ERROR );
			}
			break;
		}

		/* perform the next step of the sasl bind */
		do {
			saslrc = sasl_client_step( ctx,
				(scred == NULL) ? NULL : scred->bv_val,
				(scred == NULL) ? 0 : scred->bv_len,
				&prompts,
				(const char **)&ccred.bv_val,
				&credlen );

			if( saslrc == SASL_INTERACT &&
			    (callback)(ld, flags, defaults, prompts)
							!= LDAP_SUCCESS ) {
				break;
			}
		} while ( saslrc == SASL_INTERACT );

		ccred.bv_len = credlen;
		ber_bvfree( scred );

		if ( (saslrc != SASL_OK) && (saslrc != SASL_CONTINUE) ) {
			return( nsldapi_sasl_cvterrno( ld, saslrc ) );
		}
	} while ( rc == LDAP_SASL_BIND_IN_PROGRESS );

	if ( rc != LDAP_SUCCESS ) {
		return( rc );
	}

	if ( saslrc != SASL_OK ) {
		return( nsldapi_sasl_cvterrno( ld, saslrc ) );
	}

	saslrc = sasl_getprop( ctx, SASL_SSF, (const void **) &ssf );
	if( saslrc == SASL_OK ) {
		if( ssf && *ssf ) {
			LDAPDebug(LDAP_DEBUG_TRACE,
				"SASL install encryption, for SSF: %lu\n",
				(unsigned long) *ssf, 0, 0 );
			nsldapi_sasl_install( ld, ld->ld_conns->lconn_sb, ctx );
		}
	}

	return( rc );
}

#ifdef LDAP_SASLIO_GET_MECHS_FROM_SERVER
/*
 * Get available SASL Mechanisms supported by the server
 */

static int
nsldapi_get_sasl_mechs ( LDAP *ld, char **pmech )
{
	char *attr[] = { "supportedSASLMechanisms", NULL };
	char **values, **v, *mech, *m;
	LDAPMessage *res, *e;
	struct timeval	timeout;
	int slen, rc;

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	timeout.tv_sec = SEARCH_TIMEOUT_SECS;
	timeout.tv_usec = 0;

	rc = ldap_search_st( ld, "", LDAP_SCOPE_BASE,
		"objectclass=*", attr, 0, &timeout, &res );

	if ( rc != LDAP_SUCCESS ) {
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}
		
	e = ldap_first_entry( ld, res );
	if ( e == NULL ) {
		ldap_msgfree( res );
		if ( ld->ld_errno == LDAP_SUCCESS ) {
			LDAP_SET_LDERRNO( ld, LDAP_NO_SUCH_OBJECT, NULL, NULL );
		}
		return( LDAP_GET_LDERRNO( ld, NULL, NULL ) );
	}

	values = ldap_get_values( ld, e, "supportedSASLMechanisms" );
	if ( values == NULL ) {
		ldap_msgfree( res );
		LDAP_SET_LDERRNO( ld, LDAP_NO_SUCH_ATTRIBUTE, NULL, NULL );
		return( LDAP_NO_SUCH_ATTRIBUTE );
	}

	slen = 0;
	for(v = values; *v != NULL; v++ ) {
		slen += strlen(*v) + 1;
	}
	if ( (mech = NSLDAPI_CALLOC(1, slen)) == NULL) {
		ldap_value_free( values );
		ldap_msgfree( res );
		LDAP_SET_LDERRNO( ld, LDAP_NO_MEMORY, NULL, NULL );
		return( LDAP_NO_MEMORY );
	} 
	m = mech;
	for(v = values; *v; v++) {
		if (v != values) {
			*m++ = ' ';
		}
		slen = strlen(*v);
		strncpy(m, *v, slen);
		m += slen;
	}
	*m = '\0';

	ldap_value_free( values );
	ldap_msgfree( res );

	*pmech = mech;

	return( LDAP_SUCCESS );
}
#endif

int nsldapi_sasl_secprops(
	const char *in,
	sasl_security_properties_t *secprops )
{
	int i;
	char **props = NULL;
	char *inp;
	unsigned sflags = 0;
	sasl_ssf_t max_ssf = 0;
	sasl_ssf_t min_ssf = 0;
	unsigned maxbufsize = 0;
	int got_sflags = 0;
	int got_max_ssf = 0;
	int got_min_ssf = 0;
	int got_maxbufsize = 0;

	if (in == NULL) {
		return LDAP_PARAM_ERROR;
	}
	inp = nsldapi_strdup(in);
	if (inp == NULL) {
		return LDAP_PARAM_ERROR;
	}
	props = ldap_str2charray( inp, "," );
	NSLDAPI_FREE( inp );
	
	if( props == NULL || secprops == NULL ) {
		return LDAP_PARAM_ERROR;
	}

	for( i=0; props[i]; i++ ) {
		if( strcasecmp(props[i], "none") == 0 ) {
			got_sflags++;

		} else if( strcasecmp(props[i], "noactive") == 0 ) {
			got_sflags++;
			sflags |= SASL_SEC_NOACTIVE;

		} else if( strcasecmp(props[i], "noanonymous") == 0 ) {
			got_sflags++;
			sflags |= SASL_SEC_NOANONYMOUS;

		} else if( strcasecmp(props[i], "nodict") == 0 ) {
			got_sflags++;
			sflags |= SASL_SEC_NODICTIONARY;

		} else if( strcasecmp(props[i], "noplain") == 0 ) {
			got_sflags++;
			sflags |= SASL_SEC_NOPLAINTEXT;

		} else if( strcasecmp(props[i], "forwardsec") == 0 ) {
			got_sflags++;
			sflags |= SASL_SEC_FORWARD_SECRECY;

		} else if( strcasecmp(props[i], "passcred") == 0 ) {
			got_sflags++;
			sflags |= SASL_SEC_PASS_CREDENTIALS;

		} else if( strncasecmp(props[i],
			"minssf=", sizeof("minssf")) == 0 ) {
			if( isdigit( props[i][sizeof("minssf")] ) ) {
				got_min_ssf++;
				min_ssf = atoi( &props[i][sizeof("minssf")] );
			} else {
				return LDAP_NOT_SUPPORTED;
			}

		} else if( strncasecmp(props[i],
			"maxssf=", sizeof("maxssf")) == 0 ) {
			if( isdigit( props[i][sizeof("maxssf")] ) ) {
				got_max_ssf++;
				max_ssf = atoi( &props[i][sizeof("maxssf")] );
			} else {
				return LDAP_NOT_SUPPORTED;
			}

		} else if( strncasecmp(props[i],
			"maxbufsize=", sizeof("maxbufsize")) == 0 ) {
			if( isdigit( props[i][sizeof("maxbufsize")] ) ) {
				got_maxbufsize++;
				maxbufsize = atoi( &props[i][sizeof("maxbufsize")] );
				if( maxbufsize &&
				    (( maxbufsize < SASL_MIN_BUFF_SIZE )
				    || (maxbufsize > SASL_MAX_BUFF_SIZE ))) {
					return( LDAP_PARAM_ERROR );
				}
			} else {
				return( LDAP_NOT_SUPPORTED );
			}
		} else {
			return( LDAP_NOT_SUPPORTED );
		}
	}

	if(got_sflags) {
		secprops->security_flags = sflags;
	}
	if(got_min_ssf) {
		secprops->min_ssf = min_ssf;
	}
	if(got_max_ssf) {
		secprops->max_ssf = max_ssf;
	}
	if(got_maxbufsize) {
		secprops->maxbufsize = maxbufsize;
	}

	ldap_charray_free( props );
	return( LDAP_SUCCESS );
}

/*
 * SASL Authentication Interface: ldap_sasl_interactive_bind_s
 *
 * This routine takes a DN, SASL mech list, and a SASL callback
 * and performs the necessary sequencing to complete a SASL bind
 * to the LDAP connection ld.  The user provided callback can
 * use an optionally provided set of default values to complete
 * any necessary interactions.
 *
 * Currently inpose the following restrictions:
 *   A mech list must be provided, only LDAP_SASL_INTERACTIVE
 *   mode is supported
 */
int
LDAP_CALL
ldap_sasl_interactive_bind_s( LDAP *ld, const char *dn,
	const char *saslMechanism,
	LDAPControl **sctrl, LDAPControl **cctrl, unsigned flags,
	LDAP_SASL_INTERACT_PROC *callback, void *defaults )
{
#ifdef LDAP_SASLIO_GET_MECHS_FROM_SERVER
	char *smechs;
#endif
	int rc;

	LDAPDebug(LDAP_DEBUG_TRACE, "ldap_sasl_interactive_bind_s\n", 0, 0, 0);

	if ( !NSLDAPI_VALID_LDAP_POINTER( ld )) {
		return( LDAP_PARAM_ERROR );
	}

	if (flags != LDAP_SASL_INTERACTIVE || callback == NULL) {
		return( LDAP_PARAM_ERROR );
	}

	LDAP_MUTEX_LOCK(ld, LDAP_SASL_LOCK );

	if( saslMechanism == NULL || *saslMechanism == '\0' ) {
#ifdef LDAP_SASLIO_GET_MECHS_FROM_SERVER
		rc = nsldapi_get_sasl_mechs( ld, &smechs );
		if( rc != LDAP_SUCCESS ) {
			LDAP_MUTEX_UNLOCK(ld, LDAP_SASL_LOCK );
			return( rc );
		}
		saslMechanism = smechs;
#else
		LDAP_MUTEX_UNLOCK(ld, LDAP_SASL_LOCK );
		return( LDAP_PARAM_ERROR );
#endif
	}

#ifndef DONT_DELAY_SASL_INIT
	/* initialize SASL library */
	if ( nsldapi_sasl_init() < 0 ) {
	    return( LDAP_PARAM_ERROR );
	}
#endif

	rc = nsldapi_sasl_do_bind( ld, dn, saslMechanism,
			flags, callback, defaults, sctrl, cctrl);

	LDAP_MUTEX_UNLOCK(ld, LDAP_SASL_LOCK );
	return( rc );
}

#endif
