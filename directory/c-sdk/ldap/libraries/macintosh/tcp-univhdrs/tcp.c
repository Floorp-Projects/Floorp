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
 *	Copyright (c) 1990-92 Regents of the University of Michigan.
 *	All rights reserved.
 */
/*
 *	tcp.c -- TCP communication related routines
 */
#include "lber.h"
#include "ldap.h"
#include "tcp.h"

#include <Devices.h>
#include <Events.h>		/* to get SystemTask() */
#include <Memory.h>
#include <Errors.h>
#include <Gestalt.h>

/*
 * local prototypes
 */
#ifdef NEEDPROTOS
void		bzero( char *buf, unsigned long count );
pascal void setdoneflag( struct hostInfo *hip, char *donep );
pascal void tcp_asynchnotify( StreamPtr tstream, unsigned short event, Ptr userdatap,
							unsigned short term_reason, struct ICMPReport *icmpmsg );
OSErr		kick_mactcp( short *drefnump );
#ifdef SUPPORT_OPENTRANSPORT
pascal void EventHandler(tcpstream *tsp, OTEventCode event, OTResult result, void * /* cookie */);
#endif /* SUPPORT_OPENTRANSPORT */
#else /* NEEDPROTOS */
void		bzero();
pascal void setdoneflag();
pascal void tcp_asynchnotify();
OSErr		kick_mactcp();
#ifdef SUPPORT_OPENTRANSPORT
pascal void EventHandler();
#endif /* SUPPORT_OPENTRANSPORT */
#endif /* NEEDPROTOS */

#ifdef SUPPORT_OPENTRANSPORT
static Boolean gHaveOT = false;
#endif /* SUPPORT_OPENTRANSPORT */


#ifdef SUPPORT_OPENTRANSPORT
/*
 * Initialize the tcp module.  This mainly consists of seeing if we have
 * Open Transport and initializing it and setting our global saying so.
 */ 
OSStatus
tcp_init( void )
{
	static Boolean	sInitialized = false;
	Boolean 		hasOT;
	long 			gestaltResult;
	OSStatus		otErr = kOTNoError;
	OSErr			err;
	
	if ( sInitialized ) {
		return otErr;
	}
	
	err = Gestalt(gestaltOpenTpt, &gestaltResult);
	
	hasOT = (err == noErr) && ((gestaltResult & gestaltOpenTptPresentMask) != 0);
	hasOT = hasOT && ((gestaltResult & gestaltOpenTptTCPPresentMask) != 0);

	sInitialized = true;
	
	if ( hasOT ) {
		gHaveOT = ( InitOpenTransport() == noErr );
	}

	return otErr;
}


Boolean
tcp_have_opentransport( void )
{
	return( gHaveOT );
}
#endif /* SUPPORT_OPENTRANSPORT */


/*
 * open and return an pointer to a TCP stream (return NULL if error)
 * "buf" is a buffer for receives of length "buflen."
 */
tcpstream *
tcpopen( unsigned char * buf, long buflen ) {
	TCPiopb			pb;
	OSStatus		err;
	tcpstream *		tsp;
	short			drefnum;
#ifdef SUPPORT_OPENTRANSPORT
	TEndpointInfo	info;
#endif /* SUPPORT_OPENTRANSPORT */

	if (nil == (tsp = (tcpstream *)NewPtrClear(sizeof(tcpstream)))) {
		return( nil );
	}
	
#ifdef SUPPORT_OPENTRANSPORT
	if ( gHaveOT ) {
	
		//
		// Now create a TCP
		//
		tsp->tcps_ep = OTOpenEndpoint( OTCreateConfiguration( kTCPName ), 0, &info, &err );
		if ( !tsp->tcps_ep ) {
			if ( err == kOTNoError ) {
				err = -1;
			}
		}
	
		if ( !err ) {
			err = OTSetSynchronous( tsp->tcps_ep );
		}
	
		tsp->tcps_data = 0;
		tsp->tcps_terminated = tsp->tcps_connected = false;
		
		//
		// Install notifier we're going to use
		//
		if ( !err ) {
			err = OTInstallNotifier( tsp->tcps_ep, (OTNotifyProcPtr) EventHandler, 0 );
		}
	
		if ( err != kOTNoError ) {
			if ( tsp->tcps_ep ) {
				OTCloseProvider( tsp->tcps_ep );
			}
			DisposePtr( (Ptr)tsp );
			tsp = nil;
		}
	} else {
#endif /* SUPPORT_OPENTRANSPORT */

		if ( kick_mactcp( &drefnum ) != noErr ) {
			return ( nil );
		}
	
		if (( tsp->tcps_notifyupp = NewTCPNotifyProc( tcp_asynchnotify )) == NULL ) {
			DisposePtr( (Ptr)tsp );
			return( nil );
		}
		
		tsp->drefnum = drefnum;
		
		if ( buflen == 0 ) {
			buflen = TCP_BUFSIZ;
		}
		
		if ( buf == NULL && 
			 (nil == ( buf = tsp->tcps_buffer = (unsigned char *)NewPtr( buflen )))) {
			DisposeRoutineDescriptor( (UniversalProcPtr)tsp->tcps_notifyupp );
			DisposePtr( (Ptr)tsp );
			return( nil );
		}
		bzero( (char *)&pb, sizeof( pb ));	
		pb.csCode = TCPCreate;
		pb.ioCRefNum = tsp->drefnum;
		pb.csParam.create.rcvBuff = (Ptr) buf;
		pb.csParam.create.rcvBuffLen = buflen;
		pb.csParam.create.notifyProc = tsp->tcps_notifyupp;
		pb.csParam.create.userDataPtr = (Ptr)tsp;
			
		if (( err = PBControl( (ParmBlkPtr)&pb, 0 )) != noErr || pb.ioResult != noErr ) {
			DisposeRoutineDescriptor( (UniversalProcPtr)tsp->tcps_notifyupp );
			DisposePtr( (Ptr)tsp->tcps_buffer );
			DisposePtr( (Ptr)tsp );
			return( nil );
		}
		
		tsp->tcps_data = 0;
		tsp->tcps_terminated = tsp->tcps_connected = false;
		tsp->tcps_sptr = pb.tcpStream;

#ifdef SUPPORT_OPENTRANSPORT
	}
#endif /* SUPPORT_OPENTRANSPORT */
	
	return( tsp );
}

/*
 * connect to remote host at IP address "addr", TCP port "port"
 * return local port assigned, 0 if error
 */
#ifdef SUPPORT_OPENTRANSPORT
InetPort
tcpconnect( tcpstream * tsp, InetHost addr, InetPort port ) {
#else /* SUPPORT_OPENTRANSPORT */
ip_port
tcpconnect( tcpstream * tsp, ip_addr addr, ip_port port ) {
#endif /* SUPPORT_OPENTRANSPORT */
	TCPiopb		pb;
	OSStatus	err;
#ifdef SUPPORT_OPENTRANSPORT
	struct InetAddress sndsin, rcvsin, retsin;
	TCall		sndcall, rcvcall;
	TBind		ret;
#endif /* SUPPORT_OPENTRANSPORT */

#ifdef SUPPORT_OPENTRANSPORT
	if ( gHaveOT ) {
	
		if ( tsp->tcps_ep == NULL ) {
			return( 0 );
		}
	
		bzero( (char *)&sndsin, sizeof( struct InetAddress ));
		bzero( (char *)&rcvsin, sizeof( struct InetAddress ));
		bzero( (char *)&retsin, sizeof( struct InetAddress ));
		bzero( (char *)&sndcall, sizeof( TCall ));
		bzero( (char *)&rcvcall, sizeof( TCall));
		bzero( (char *)&ret, sizeof( TBind ));
		
		// Bind TCP to an address and port.  We don't care which one, so we pass null for
		// the requested address.
		ret.addr.maxlen = sizeof( struct InetAddress );
		ret.addr.buf = (unsigned char *)&retsin;
		err = OTBind( tsp->tcps_ep, NULL, &ret );
		if ( err != kOTNoError ) {
			return( 0 );
		}
	
		OTInitInetAddress( &sndsin, port, addr );
		sndcall.addr.len = sizeof( struct InetAddress );
		sndcall.addr.buf = (UInt8 *)&sndsin;
	
		rcvcall.addr.maxlen = sizeof( struct InetAddress );
		rcvcall.addr.buf = (unsigned char *)&rcvsin;
	
		err = OTConnect( tsp->tcps_ep, &sndcall, &rcvcall );
		if ( err != kOTNoError ) {
			return 0;
		}
	
		tsp->tcps_connected = true;
		tsp->tcps_remoteport = rcvsin.fPort;
		tsp->tcps_remoteaddr = rcvsin.fHost;
		return( retsin.fPort );
	
	} else {
#endif /* SUPPORT_OPENTRANSPORT */
	
		if ( tsp->tcps_sptr == (StreamPtr)NULL ) {
			return( 0 );
		}
			
		bzero( (char *)&pb, sizeof( pb ));	
		pb.csCode = TCPActiveOpen;
		pb.ioCRefNum = tsp->drefnum;
		pb.tcpStream = tsp->tcps_sptr;
		pb.csParam.open.remoteHost = addr;
		pb.csParam.open.remotePort = port;
		pb.csParam.open.ulpTimeoutValue = 15;
		pb.csParam.open.validityFlags = timeoutValue;
			
		if (( err = PBControl( (ParmBlkPtr)&pb, 0 )) != noErr || pb.ioResult != noErr ) {
			return( 0 );
		}
		
		tsp->tcps_connected = true;
		return( pb.csParam.open.localPort );
	
#ifdef SUPPORT_OPENTRANSPORT
	}
#endif /* SUPPORT_OPENTRANSPORT */
	
}


/*
 * close and release a TCP stream
 * returns 0 if no error, -1 if any error occurs
 */
short
tcpclose( tcpstream * tsp ) {
	TCPiopb		pb;
	OSStatus		rc;
	
#ifdef SUPPORT_OPENTRANSPORT
	if ( gHaveOT ) {
	
	
		if ( tsp->tcps_ep == NULL ) {
			return( -1 );
		}
	
#ifdef notdef
		/*
		 * if connected execute a close
		 */
		if ( tcp->tcps_connected ) {
			Call OTSndOrderlyDisconnect and wait for the other end to respond.  This requires
			waiting for and reading any data that comes in in the meantime.  This code was ifdefed
			out in the MacTCP so it is here too.
		}
#endif /* notdef */
		
		rc = OTSndDisconnect( tsp->tcps_ep, NULL );
	
		OTCloseProvider( tsp->tcps_ep );
		DisposePtr( (Ptr)tsp );
		
		if ( rc != 0 ) {
			rc = -1;
		}

	} else {
#endif /* SUPPORT_OPENTRANSPORT */
	
		if ( tsp->tcps_sptr == (StreamPtr)NULL ) {
			return( -1 );
		}
		
#ifdef notdef
		/*
		 * if connected execute a close
		 */
		if ( tcp->tcps_connected ) {
			bzero( (char *)&pb, sizeof( pb ));
			pb.csCode = TCPClose;
			pb.ioCRefNum = tsp->drefnum;
			pb.tcpStream = sp;
			pb.csParam.close.validityFlags = 0;
			PBControl( (ParmBlkPtr)&pb, 0 );
		}
#endif /* notdef */
			
		bzero( (char *)&pb, sizeof( pb ));
		pb.csCode = TCPRelease;
		pb.ioCRefNum = tsp->drefnum;
		pb.tcpStream = tsp->tcps_sptr;
		pb.csParam.close.validityFlags = 0;
		rc = 0;
		
		if ( PBControl( (ParmBlkPtr)&pb, 0 ) != noErr || pb.ioResult != noErr ) {
			rc = -1;
		}
		
		DisposeRoutineDescriptor( (UniversalProcPtr)tsp->tcps_notifyupp );
		DisposePtr( (Ptr)tsp->tcps_buffer );
		DisposePtr( (Ptr)tsp );
	
#ifdef SUPPORT_OPENTRANSPORT
	}
#endif /* SUPPORT_OPENTRANSPORT */
		
	return( rc );	
}


/*
 * wait for new data to arrive
 *
 * if "timeout" is NULL, wait until data arrives or connection goes away
 * if "timeout" is a pointer to a zero'ed struct, poll
 * else wait for "timeout->tv_sec + timeout->tv_usec" seconds
 */
short
tcpselect( tcpstream * tsp, struct timeval * timeout )
{
	unsigned long ticks, endticks;
	short rc;

	if ( timeout != NULL ) {
		endticks = 60 * timeout->tv_sec + ( 60 * timeout->tv_usec ) / 1000000 + TickCount();
	}
	ticks = 0;	

	while (( rc = tcpreadready( tsp )) == 0 && ( timeout == NULL || ticks < endticks )) {
		Delay( 2L, &ticks );
		SystemTask();
	}

	return ( rc );
}


short
tcpreadready( tcpstream *tsp )
{
#ifdef SUPPORT_OPENTRANSPORT
	size_t 	dataAvail;

	if (gHaveOT) {
		if ( tsp->tcps_ep == NULL ) {
			return( -1 );
		}
	
		OTCountDataBytes( tsp->tcps_ep, &dataAvail );
		tsp->tcps_data = ( dataAvail != 0 );	
	} else {
#endif /* SUPPORT_OPENTRANSPORT */
		if ( tsp->tcps_sptr == (StreamPtr)NULL ) {
			return( -1 );
		}
	
		/* tsp->tcps_data is set in async. notify proc, so nothing for us to do here */
#ifdef SUPPORT_OPENTRANSPORT
	}
#endif /* SUPPORT_OPENTRANSPORT */
	return ( tsp->tcps_terminated ? -1 : ( tsp->tcps_data < 1 ) ? 0 : 1 );
}


/*
 * read up to "rbuflen" bytes into "rbuf" from a connected TCP stream
 * wait up to "timeout" seconds for data (if timeout == 0, wait "forever")
 */
long
tcpread( tcpstream * tsp, UInt8 timeout, unsigned char * rbuf,
						  unsigned short rbuflen, DialogPtr dlp ) {
#pragma unused (dlp)
	TCPiopb			pb;
	unsigned long	time, end_time;
#ifdef SUPPORT_OPENTRANSPORT
	OTFlags			flags;
	OTResult		result;
	size_t 			dataAvail;
#endif /* SUPPORT_OPENTRANSPORT */
	
#ifdef SUPPORT_OPENTRANSPORT
	if ( gHaveOT ) {
	
		if ( tsp->tcps_ep == NULL ) {
			return( -1 );
		}
		
		// Try to read data.  Since we're in non-blocking mode, this will fail with kOTNoDataErr
		// if no data is available.
		result = OTRcv( tsp->tcps_ep, rbuf, rbuflen, &flags );
		if ( result == kOTNoDataErr ) {
			// Nothing available, wait for some.  The ugly spin loop below is the way the old
			// MacTCP code worked.  I should fix it, but I don't have time right now.
			tsp->tcps_data = 0;
			GetDateTime( &time );
			end_time = time + timeout;
		
			while (( timeout <= 0 || end_time > time ) && tsp->tcps_data < 1 && !tsp->tcps_terminated ) {
				OTCountDataBytes( tsp->tcps_ep, &dataAvail );
				if ( dataAvail > 0 ) {
					tsp->tcps_data = 1;
				}
				GetDateTime( &time );
				SystemTask();
			}
			
			if ( tsp->tcps_data < 1 ) {
				return( tsp->tcps_terminated ? -3 : -1 );
			}
			
			// Should have data available now, try again.
			result = OTRcv( tsp->tcps_ep, rbuf, rbuflen, &flags );
		}
		
		if ( result < 0 ) {
			return( -1 );
		}
		
		OTCountDataBytes( tsp->tcps_ep, &dataAvail );
		if ( dataAvail == 0 ) {
			tsp->tcps_data = 0;
		}

		return( result );
	
	} else {
#endif /* SUPPORT_OPENTRANSPORT */
	
		if ( tsp->tcps_sptr == (StreamPtr)NULL ) {
			return( -1 );
		}
		
		GetDateTime( &time );
		end_time = time + timeout;
	
		while(( timeout <= 0 || end_time > time ) && tsp->tcps_data < 1 &&
				!tsp->tcps_terminated ) {
			GetDateTime( &time );
			SystemTask();
		}
		
		if ( tsp->tcps_data < 1 ) {
			return( tsp->tcps_terminated ? -3 : -1 );
		}
			
		bzero( (char *)&pb, sizeof( pb ));	
		pb.csCode = TCPRcv;
		pb.ioCRefNum = tsp->drefnum;
		pb.tcpStream = tsp->tcps_sptr;
		pb.csParam.receive.commandTimeoutValue = timeout;
		pb.csParam.receive.rcvBuff = (char *)rbuf;	
		pb.csParam.receive.rcvBuffLen = rbuflen;
		
		if ( PBControl( (ParmBlkPtr)&pb, 0 ) != noErr || pb.ioResult != noErr ) {
			return( -1 );
		}
		
		if ( --(tsp->tcps_data) < 0 ) {
			tsp->tcps_data = 0;
		}
		
		return( pb.csParam.receive.rcvBuffLen );

#ifdef SUPPORT_OPENTRANSPORT		
	}	
#endif /* SUPPORT_OPENTRANSPORT */
}

/*
 * send TCP data
 * "sp" is the stream to write to
 * "wbuf" is "wbuflen" bytes of data to send
 * returns < 0 if error, number of bytes written if no error
 */
long
tcpwrite( tcpstream * tsp, unsigned char * wbuf, unsigned short wbuflen ) {
	TCPiopb		pb;
	wdsEntry	wds[ 2 ];
	OSErr 		err;
#ifdef SUPPORT_OPENTRANSPORT		
	OTResult 	result;
#endif /* SUPPORT_OPENTRANSPORT */
	
#ifdef SUPPORT_OPENTRANSPORT		
	if ( gHaveOT ) {
	
		if ( tsp->tcps_ep == NULL ) {
			return( -1 );
		}
		
		OTSetBlocking( tsp->tcps_ep );
		result = OTSnd( tsp->tcps_ep, wbuf, wbuflen, 0 );
		OTSetNonBlocking( tsp->tcps_ep );
		return( result );
		
	} else {
#endif /* SUPPORT_OPENTRANSPORT */
	
		if ( tsp->tcps_sptr == (StreamPtr)NULL ) {
			return( -1 );
		}
		
		wds[ 0 ].length = wbuflen;
		wds[ 0 ].ptr = (char *)wbuf;
		wds[ 1 ].length = 0;
		
		bzero( (char *)&pb, sizeof( pb ));	
		pb.csCode = TCPSend;
		pb.ioCRefNum = tsp->drefnum;
		pb.tcpStream = tsp->tcps_sptr;
		pb.csParam.send.wdsPtr = (Ptr)wds;
		pb.csParam.send.validityFlags = 0;
		
		if (( err = PBControl( (ParmBlkPtr)&pb, 0 )) != noErr || pb.ioResult != noErr ) {
			return( -1 );
		}
		
		return( (long) wbuflen );

#ifdef SUPPORT_OPENTRANSPORT		
	}
#endif /* SUPPORT_OPENTRANSPORT */
}

static pascal void
tcp_asynchnotify(
	StreamPtr			tstream,
	unsigned short		event,
	Ptr					userdatap,
	unsigned short		term_reason,	
	struct ICMPReport	*icmpmsg
)
{
#pragma unused (tstream, term_reason, icmpmsg)
	tcpstream	*tsp;
	
	tsp = (tcpstream *)userdatap;
	switch( event ) {
		case TCPDataArrival:
			++(tsp->tcps_data);
			break;
		case TCPClosing:
		case TCPTerminate:
		case TCPULPTimeout:
			tsp->tcps_terminated = true;
			break;
		default:
			break;
	}
}


short
tcpgetpeername( tcpstream *tsp, ip_addr *addrp, tcp_port *portp ) {
	TCPiopb		pb;
	OSErr		err;

#ifdef SUPPORT_OPENTRANSPORT		
	if ( gHaveOT ) {
	
		if ( tsp->tcps_ep == NULL ) {
			return( -1 );
		}
			
		if ( addrp != NULL ) {
			*addrp = tsp->tcps_remoteaddr;
		}
	
		if ( portp != NULL ) {
			*portp = tsp->tcps_remoteport;
		}
		
	} else {
#endif /* SUPPORT_OPENTRANSPORT */
	
		if ( tsp->tcps_sptr == (StreamPtr)NULL ) {
			return( -1 );
		}
			
		bzero( (char *)&pb, sizeof( pb ));	
		pb.csCode = TCPStatus;
		pb.ioCRefNum = tsp->drefnum;
		pb.tcpStream = tsp->tcps_sptr;
		
		if (( err = PBControl( (ParmBlkPtr)&pb, 0 )) != noErr || pb.ioResult != noErr ) {
			return( err );
		}
	
		if ( addrp != NULL ) {
			*addrp = pb.csParam.status.remoteHost;
		}
	
		if ( portp != NULL ) {
			*portp = pb.csParam.status.remotePort;
		}

#ifdef SUPPORT_OPENTRANSPORT		
	}

	return( kOTNoError );
#else /* SUPPORT_OPENTRANSPORT */
	return( noErr );
#endif /* SUPPORT_OPENTRANSPORT */
}


/*
 * bzero -- set "len" bytes starting at "p" to zero
 */
static void
bzero( char *p, unsigned long len )
{
	unsigned long	i;
	
	for ( i = 0; i < len; ++i ) {
		*p++ = '\0';
	}
}


pascal void
setdoneflag( struct hostInfo *hip, char *donep )
{
	++(*donep);
#pragma unused (hip)
}


/*
 * return a hostInfo structure for "host" (return != noErr if error)
 */
short
#ifdef SUPPORT_OPENTRANSPORT		
gethostinfobyname( char *host, InetHostInfo *hip ) {
#else /* SUPPORT_OPENTRANSPORT */
gethostinfobyname( char *host, struct hostInfo *hip ) {
#endif /* SUPPORT_OPENTRANSPORT */
	char					done = 0;
	OSStatus				err;
	unsigned long			time;
	ResultUPP				rupp;
#ifdef notdef
	struct dnr_struct		dnr_global = {nil, 0};
#endif /* notdef */
#ifdef SUPPORT_OPENTRANSPORT		
	hostInfo				hi;				/* Old MacTCP version of hostinfo */
	InetSvcRef				inetsvc;		/* Internet services reference */
#endif /* SUPPORT_OPENTRANSPORT */

#ifdef SUPPORT_OPENTRANSPORT		
	if ( gHaveOT ) {
	
		// Get an Internet Services reference
		inetsvc = OTOpenInternetServices( kDefaultInternetServicesPath, 0, &err );
		if ( !inetsvc || err != kOTNoError ) {
			if ( err == kOTNoError ) {
				err = -1;
			}
			inetsvc = nil;
		}
		
		if ( !err ) {	
			err = OTInetStringToAddress( inetsvc, host, hip );
		}
		
		if ( inetsvc ) {
			OTCloseProvider(inetsvc);
		}
		
	} else {
#endif /* SUPPORT_OPENTRANSPORT */
	
		kick_mactcp( NULL );
	
		if (( err = OpenResolver( /* &dnr_global, */ NULL )) != noErr )
			return( err );
	
		if (( rupp = NewResultProc( setdoneflag )) == NULL ) {
			err = memFullErr;
		} else {
#ifdef SUPPORT_OPENTRANSPORT		
			bzero( (char *)&hi, sizeof( hostInfo ));
			if (( err = StrToAddr( /* &dnr_global, */ host, &hi, rupp, &done ))  == cacheFault ) {
#else /* SUPPORT_OPENTRANSPORT */
			bzero((char *) hip, sizeof( hostInfo ));
			if (( err = StrToAddr( /* &dnr_global, */ host, hip, rupp, &done ))  == cacheFault ) {
#endif /* SUPPORT_OPENTRANSPORT */
				while( !done ) {
					Delay( 2L, &time );			// querying DN servers; wait for reply
					SystemTask();
				}
#ifdef SUPPORT_OPENTRANSPORT		
				err = hi.rtnCode;
#else /* SUPPORT_OPENTRANSPORT */
				err = hip->rtnCode;
#endif /* SUPPORT_OPENTRANSPORT */
			}
			DisposeRoutineDescriptor( (UniversalProcPtr)rupp );
		}
	
#if !defined(MOZILLA_CLIENT)
		CloseResolver( /* &dnr_global */ );
#endif
		
#ifdef SUPPORT_OPENTRANSPORT		
		/* Copy the results to the InetHostInfo area passed in by caller */
		BlockMove(hi.cname, hip->name, kMaxHostNameLen);
		BlockMove(hi.addr, hip->addrs, 4*NUM_ALT_ADDRS);
		hip->addrs[NUM_ALT_ADDRS] = 0;
		
		/* Convert the return code to an OT style return code */
		if ( err == nameSyntaxErr ) {
			err = kOTBadNameErr;
		} else if ( err == noNameServer || err == authNameErr || err == noAnsErr ) {
			err = kOTBadNameErr;
		} else if (err != noErr) {
			err = kOTSysErrorErr;
		}
	
	}

	if (( err == kOTNoError ) && ( hip->addrs[ 0 ] == 0 )) {
		err = kOTBadNameErr;
	}
	return( err );

#else /* SUPPORT_OPENTRANSPORT */
	if ( err != noErr || (( err == noErr ) && ( hip->addr[ 0 ] == 0 ))) {
		return( err == noErr ? noAnsErr : err );
  	}
	return( noErr );
#endif /* SUPPORT_OPENTRANSPORT */
}

/*
 * return a hostInfo structure for "addr" (return != noErr if error)
 */
short
#ifdef SUPPORT_OPENTRANSPORT		
gethostinfobyaddr( InetHost addr, InetHostInfo *hip )
#else /* SUPPORT_OPENTRANSPORT */
gethostinfobyaddr( ip_addr addr, struct hostInfo *hip )
#endif /* SUPPORT_OPENTRANSPORT */
{
	
	char					done = 0;
	OSStatus				err;
	unsigned long			time;
	ResultUPP				rupp;
#ifdef notdef
	struct dnr_struct		dnr_global = {nil, 0};
#endif /* notdef */
#ifdef SUPPORT_OPENTRANSPORT		
	hostInfo				hi;				/* Old MacTCP version of hostinfo */
	InetSvcRef				inetsvc;		/* Internet services reference */
#endif /* SUPPORT_OPENTRANSPORT */

#ifdef SUPPORT_OPENTRANSPORT		
	if ( gHaveOT ) {
		// Get an Internet Services reference
		inetsvc = OTOpenInternetServices( kDefaultInternetServicesPath, 0, &err );
		if ( !inetsvc || err != kOTNoError ) {
			if ( err == kOTNoError ) {
				err = -1;
			}
			inetsvc = nil;
		}
		
		if ( !err ) {
			err = OTInetAddressToName( inetsvc, addr, hip->name );
		}
		
		if ( inetsvc ) {
			OTCloseProvider( inetsvc );
		}
	
	} else {
#endif /* SUPPORT_OPENTRANSPORT */
	
		kick_mactcp( NULL );
	
		if (( err = OpenResolver( /* &dnr_global, */ NULL )) != noErr )
			return( err );
	
		if (( rupp = NewResultProc( setdoneflag )) == NULL ) {
			err = memFullErr;
		} else {
#ifdef SUPPORT_OPENTRANSPORT		
			bzero( (char *) &hi, sizeof( hostInfo ));
			if (( err = AddrToName( /* &dnr_global, */ addr, &hi, rupp, &done ))  == cacheFault ) {
#else /* SUPPORT_OPENTRANSPORT */
			bzero( (char *)hip, sizeof( hostInfo ));
			if (( err = AddrToName( /* &dnr_global, */ addr, hip, rupp, &done ))  == cacheFault ) {
#endif /* SUPPORT_OPENTRANSPORT */
				while( !done ) {
					Delay( 2L, &time );			// querying DN servers; wait for reply
					SystemTask();
				}
#ifdef SUPPORT_OPENTRANSPORT		
				err = hi.rtnCode;
#else /* SUPPORT_OPENTRANSPORT */
				err = hip->rtnCode;
#endif /* SUPPORT_OPENTRANSPORT */
			}
			DisposeRoutineDescriptor( (UniversalProcPtr)rupp );
		}
	
#if !defined(MOZILLA_CLIENT)
		CloseResolver( /* &dnr_global */ );
#endif
	
#ifdef SUPPORT_OPENTRANSPORT		
		/* Copy the results to the InetHostInfo area passed in by caller */
		BlockMove(hi.cname, hip->name, kMaxHostNameLen);
		BlockMove(hi.addr, hip->addrs, 4*NUM_ALT_ADDRS);
		hip->addrs[NUM_ALT_ADDRS] = 0;
		
		/* Convert the return code to an OT style return code */
		if (err == nameSyntaxErr) {
			err = kOTBadNameErr;
		} else if (err == noNameServer || err == authNameErr || err == noAnsErr) {
			err = kOTBadNameErr;
		} else if (err != noErr) {
			err = kOTSysErrorErr;
		}
	
	}
#endif /* SUPPORT_OPENTRANSPORT */
	
	return( err );
}

/*
 * return a ASCII equivalent of ipaddr.  addrstr must be at large enough to hold the
 * result which is of the form "AAA.BBB.CCC.DDD" and can be a maximum of 16 bytes in size
 */
short
#ifdef SUPPORT_OPENTRANSPORT		
ipaddr2str( InetHost ipaddr, char *addrstr )
#else /* SUPPORT_OPENTRANSPORT */
ipaddr2str( ip_addr ipaddr, char *addrstr )
#endif /* SUPPORT_OPENTRANSPORT */
{
	OSStatus				err;
#ifdef notdef
	struct dnr_struct		dnr_global = {nil, 0};
#endif /* notdef */

#ifdef SUPPORT_OPENTRANSPORT		
	if ( gHaveOT ) {
	
		OTInetHostToString( ipaddr, addrstr );
		err = kOTNoError;
	
	} else {
#endif /* SUPPORT_OPENTRANSPORT */
	
		kick_mactcp( NULL );
	
		if (( err = OpenResolver( /* &dnr_global, */ NULL )) != noErr )
			return( err );
	
		err = AddrToStr( ipaddr, addrstr );
	
#if !defined(MOZILLA_CLIENT)
		CloseResolver( /* &dnr_global */ );
#endif
		
#ifdef SUPPORT_OPENTRANSPORT		
	}
#endif /* SUPPORT_OPENTRANSPORT */
	
	return( err );
}


#ifdef SUPPORT_OPENTRANSPORT		
/*******************************************************************************
** EventHandler
********************************************************************************/
pascal void EventHandler(tcpstream *tsp, OTEventCode event, OTResult result, void * /* cookie */)
{

	switch(event)
	{
		case T_ORDREL:
		case T_DISCONNECT:
			tsp->tcps_terminated = true;
			break;
		case T_DATA:
		case T_EXDATA:
			tsp->tcps_data += 1;
			break;
		default:
			break;
	}
	return;
}
#endif /* SUPPORT_OPENTRANSPORT */


static OSErr
kick_mactcp( short *drefnump )
{
	short						dref;
	OSErr						err;
	struct GetAddrParamBlock	gapb;

/*
 * we make sure the MacTCP driver is open and issue a "Get My Address" call
 * so that adevs link MacPPP are initialized (MacTCP is dumb)
 */
	if (( err = OpenDriver( "\p.IPP", &dref )) != noErr ) {
		return( err );
	}

	if ( drefnump != NULL ) {
		*drefnump = dref;
	}

	bzero( (char *)&gapb, sizeof( gapb ));
	gapb.csCode = ipctlGetAddr;
	gapb.ioCRefNum = dref;

	err = PBControl( (ParmBlkPtr)&gapb, 0 );

	return( noErr );
}
