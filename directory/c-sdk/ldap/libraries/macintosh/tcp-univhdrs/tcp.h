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
 * tcp.h  interface to MCS's TCP routines
 */
 
#include <Dialogs.h>
#ifdef SUPPORT_OPENTRANSPORT
#include <OpenTransport.h>
#include <OpenTptInternet.h>
#endif /* SUPPORT_OPENTRANSPORT */

//#include "MacTCPCommonTypes.h"
#include "AddressXlation.h"
#include "TCPPB.h"
#include "GetMyIPAddr.h"
#include <utime.h>	/* for the UNIX-y struct timeval */

#include <MacTCP.h>

#ifndef TCP_BUFSIZ
#define TCP_BUFSIZ	8192
#endif /* TCP_BUFSIZ */

typedef struct tcpstream {
	StreamPtr			tcps_sptr;			/* stream pointer for MacTCP TCP PB calls */
#ifdef SUPPORT_OPENTRANSPORT
	EndpointRef			tcps_ep;			/* OpenTransport end point */
#endif /* SUPPORT_OPENTRANSPORT */
	short				tcps_data;			/* count of packets on read queue */
	short				drefnum;			/* driver ref num, for convenience */
	Boolean				tcps_connected;		/* true if connection was made */
	Boolean				tcps_terminated;	/* true if connection no longer exists */
#ifdef SUPPORT_OPENTRANSPORT
	InetHost			tcps_remoteaddr;	/* Address of our peer */
	InetPort			tcps_remoteport;	/* Port number of our peer */
#endif /* SUPPORT_OPENTRANSPORT */
	unsigned char		*tcps_buffer;		/* buffer given over to system to use */
	struct tcpstream	*tcps_next;			/* next one in chain */
	TCPNotifyUPP		tcps_notifyupp;		/* universal proc pointer for notify routine */
} tcpstream, *tcpstreamptr;

#define TCP_IS_TERMINATED( tsp )		(tsp)->tcps_terminated

/*
 * function prototypes
 */
#ifdef SUPPORT_OPENTRANSPORT
OSStatus	tcp_init(void);
Boolean		tcp_have_opentransport( void );
#endif /* SUPPORT_OPENTRANSPORT */
tcpstream 	*tcpopen( unsigned char * buf, long buflen );
tcp_port	tcpconnect( tcpstream *s, ip_addr addr, tcp_port port );
short		tcpclose( tcpstream *s );
long		tcpread(  tcpstream *s, UInt8 timeout, unsigned char * rbuf,
				unsigned short rbuflen, DialogPtr dlp );
long		tcpwrite( tcpstream *s, unsigned char * wbuf, unsigned short wbuflen );
short		tcpselect( tcpstream *s, struct timeval * timeout );
short		tcpreadready( tcpstream *tsp );
short		tcpgetpeername( tcpstream * tsp, ip_addr *addrp, tcp_port *portp );

#ifdef SUPPORT_OPENTRANSPORT
short		gethostinfobyname( char *host, InetHostInfo *hip );
short		gethostinfobyaddr(InetHost addr, InetHostInfo *hip );
short		ipaddr2str( InetHost ipaddr, char *addrstr );
#else /* SUPPORT_OPENTRANSPORT */
short		gethostinfobyname( char *host, struct hostInfo *hip );
short		gethostinfobyaddr( ip_addr addr, struct hostInfo *hip );
short		ipaddr2str( ip_addr ipaddr, char *addrstr );
#endif /* SUPPORT_OPENTRANSPORT */
