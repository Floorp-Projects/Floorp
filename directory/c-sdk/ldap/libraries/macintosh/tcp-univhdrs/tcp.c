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
pascal void EventHandler(void * contextPtr, OTEventCode event, OTResult result, void * /* cookie */);
pascal void DNREventHandler(void * contextPtr, OTEventCode event, OTResult result, void * /* cookie */);
#else /* NEEDPROTOS */
void		bzero();
pascal void setdoneflag();
pascal void EventHandler();
pascal void DNREventHandler();
#endif /* NEEDPROTOS */

static Boolean gHaveOT = false;
static OTNotifyUPP		sEventHandlerUPP;
static EventRecord		dummyEvent;


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
#if TARGET_CARBON
		gHaveOT = ( InitOpenTransportInContext(kInitOTForApplicationMask, NULL) == noErr );
#else /* TARGET_CARBON */
		gHaveOT = ( InitOpenTransport() == noErr );
#endif /* TARGET_CARBON */

		sEventHandlerUPP = NewOTNotifyUPP(EventHandler);
		/* Somewhere we should probably do a DisposeOTNotifyUPP(sEventHandlerUPP) but since we only
		   create one once I'm not going to worry about a leak */

	}

	return otErr;
}


Boolean
tcp_have_opentransport( void )
{
	return( gHaveOT );
}


/*
 * open and return an pointer to a TCP stream (return NULL if error)
 * "buf" is a buffer for receives of length "buflen."
 */
tcpstream *
tcpopen( unsigned char * buf, long buflen ) {
	OSStatus		err;
	tcpstream *		tsp;
	short			drefnum;
	TEndpointInfo	info;

	if (nil == (tsp = (tcpstream *)NewPtrClear(sizeof(tcpstream)))) {
		return( nil );
	}
	
	if ( gHaveOT ) {
	
		//
		// Now create a TCP
		//
#if TARGET_CARBON
		tsp->tcps_ep = OTOpenEndpointInContext( OTCreateConfiguration( kTCPName ), 0, &info, &err, NULL );
#else /* TARGET_CARBON */
		tsp->tcps_ep = OTOpenEndpoint( OTCreateConfiguration( kTCPName ), 0, &info, &err );
#endif /* TARGET_CARBON */

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
		//	need to pass tsp to make it work		%%%
		//
		if ( !err ) {
			err = OTInstallNotifier( tsp->tcps_ep, sEventHandlerUPP, tsp );
		}
	
		if ( err != kOTNoError ) {
			if ( tsp->tcps_ep ) {
				OTCloseProvider( tsp->tcps_ep );
			}
			DisposePtr( (Ptr)tsp );
			tsp = nil;
		}
	}	
	return( tsp );
}

/*
 * connect to remote host at IP address "addr", TCP port "port"
 * return local port assigned, 0 if error
 */
InetPort
tcpconnect( tcpstream * tsp, InetHost addr, InetPort port ) {
	OSStatus	err;
	struct InetAddress sndsin, rcvsin, retsin;
	TCall		sndcall, rcvcall;
	TBind		ret;
	TDiscon		localDiscon;
	unsigned long	time, end_time;
	int		timeout = 45;				/*	%%%	*/

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
	
	
		// set to async mode, then OTConnect		%%%
		err = OTSetAsynchronous( tsp->tcps_ep );

		OTInitInetAddress( &sndsin, port, addr );
		sndcall.addr.len = sizeof( struct InetAddress );
		sndcall.addr.buf = (UInt8 *)&sndsin;
		rcvcall.addr.maxlen = sizeof( struct InetAddress );
		rcvcall.addr.buf = (unsigned char *)&rcvsin;

		err = OTConnect( tsp->tcps_ep, &sndcall, nil );
		if ( (err != kOTNoDataErr) && (err != kOTNoError) )	
			return 0;
	
		// wait for the OTConnect done		%%%	
		GetDateTime( &time );
		end_time = time + timeout;
		while(( timeout <= 0 || end_time > time ) && !tsp->tcps_connected &&
				!tsp->tcps_terminated ) {
			GetDateTime( &time );
			(void)WaitNextEvent(nullEvent, &dummyEvent, 1, NULL);
		}
		
		if ( tsp->tcps_connected )
		{
			err = OTRcvConnect( tsp->tcps_ep, &rcvcall );
			if ( err == kOTNoError)
			{
				tsp->tcps_remoteport = rcvsin.fPort;
				tsp->tcps_remoteaddr = rcvsin.fHost;
			}
			OTSetSynchronous( tsp->tcps_ep );	// set back to sync	%%%	
			return( retsin.fPort );
		}
		else
		{
			if ( tsp->tcps_terminated )
				OTRcvDisconnect(tsp->tcps_ep, &localDiscon );
			return 0;
		}	
	}
}


/*
 * close and release a TCP stream
 * returns 0 if no error, -1 if any error occurs
 */
short
tcpclose( tcpstream * tsp ) {
	OSStatus		rc;
	
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

	}		
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
		(void)WaitNextEvent(nullEvent, &dummyEvent, 1, NULL);
		ticks = TickCount();
	}

	return ( rc );
}


short
tcpreadready( tcpstream *tsp )
{
	size_t 	dataAvail;

	if (gHaveOT) {
		if ( tsp->tcps_ep == NULL ) {
			return( -1 );
		}
	
		OTCountDataBytes( tsp->tcps_ep, &dataAvail );
		tsp->tcps_data = ( dataAvail != 0 );
	}

	return ( tsp->tcps_terminated ? -1 : ( tsp->tcps_data < 1 ) ? 0 : 1 );
}


short
tcpwriteready( tcpstream *tsp )
{
	if (gHaveOT) {
		if ( tsp->tcps_ep == NULL ) {
			return( -1 );
		}
	}

	return ( tsp->tcps_terminated ? -1 : 1 );
}


/*
 * read up to "rbuflen" bytes into "rbuf" from a connected TCP stream
 * wait up to "timeout" seconds for data (if timeout == 0, wait "forever")
 */
long
tcpread( tcpstream * tsp, UInt8 timeout, unsigned char * rbuf,
						  unsigned short rbuflen, DialogPtr dlp ) {
#pragma unused (dlp)
	unsigned long	time, end_time;
	OTFlags			flags;
	OTResult		result;
	size_t 			dataAvail;
	
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
				(void)WaitNextEvent(nullEvent, &dummyEvent, 1, NULL);
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
	
	}	
}

/*
 * send TCP data
 * "sp" is the stream to write to
 * "wbuf" is "wbuflen" bytes of data to send
 * returns < 0 if error, number of bytes written if no error
 */
long
tcpwrite( tcpstream * tsp, unsigned char * wbuf, unsigned short wbuflen ) {
	OSErr 		err;
	OTResult 	result;
	
	if ( gHaveOT ) {
	
		if ( tsp->tcps_ep == NULL ) {
			return( -1 );
		}
		
		OTSetBlocking( tsp->tcps_ep );
		result = OTSnd( tsp->tcps_ep, wbuf, wbuflen, 0 );
		OTSetNonBlocking( tsp->tcps_ep );
		return( result );
		
	}
}



short
tcpgetpeername( tcpstream *tsp, InetHost *addrp, InetPort *portp ) {
	OSErr		err;

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
		
	}

	return( kOTNoError );
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


/*
 * return a hostInfo structure for "host" (return != noErr if error)
 */
short
gethostinfobyname( char *host, InetHostInfo *hip ) {
	char			done = 0;
	OSStatus		err;
	unsigned long	time;
	InetSvcRef		inetsvc;		/* Internet services reference */
	unsigned long	cur_time, end_time;
	int				timeout = 20;
	static int 		sNotifiedWithResult;
	static int* 	sNpointer;
	OTNotifyUPP		DNREventHandlerUPP;
	
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
			// set to async mode				%%%
			sNotifiedWithResult = 0;
			sNpointer = &sNotifiedWithResult;
			DNREventHandlerUPP = NewOTNotifyUPP(DNREventHandler);
			err = OTInstallNotifier(inetsvc, DNREventHandlerUPP, (void*)sNpointer);
			err = OTSetAsynchronous( inetsvc );

			err = OTInetStringToAddress( inetsvc, host, hip );
			if ( err == kOTNoError ) {
				GetDateTime( &cur_time );
				end_time = cur_time + timeout;
				while(( timeout <= 0 || end_time > cur_time ) && (sNotifiedWithResult == 0)) {
					GetDateTime( &cur_time );
					(void)WaitNextEvent(nullEvent, &dummyEvent, 1, NULL);
				}
				if ( sNotifiedWithResult != 1 )
					err = -1;
			}				
			
			DisposeOTNotifyUPP(DNREventHandlerUPP);
		}
		
		if ( inetsvc ) {
			OTCloseProvider(inetsvc);
		}
	}
	
	return( err );
}

/*
 * return a hostInfo structure for "addr" (return != noErr if error)
 */
short
gethostinfobyaddr( InetHost addr, InetHostInfo *hip )
{
	
	char			done = 0;
	OSStatus		err;
	unsigned long	time;
	InetSvcRef		inetsvc;		/* Internet services reference */
	unsigned long	cur_time, end_time;
	int				timeout = 20;
	static int 		sNotifiedWithResult2;
	static int* 	sNpointer2;
	OTNotifyUPP		DNREventHandlerUPP;

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
			// set to async mode				%%%
			sNotifiedWithResult2 = 0;
			sNpointer2 = &sNotifiedWithResult2;
			DNREventHandlerUPP = NewOTNotifyUPP(DNREventHandler);
			err = OTInstallNotifier(inetsvc, DNREventHandlerUPP, (void*)sNpointer2);
			err = OTSetAsynchronous( inetsvc );

			err = OTInetAddressToName( inetsvc, addr, hip->name );
			if ( err == kOTNoError ) {
				GetDateTime( &cur_time );
				end_time = cur_time + timeout;
				while(( timeout <= 0 || end_time > cur_time ) && (sNotifiedWithResult2 == 0)) {
					GetDateTime( &cur_time );
					(void)WaitNextEvent(nullEvent, &dummyEvent, 1, NULL);
				}
				if ( sNotifiedWithResult2 != 1 )
					err = -1;

				DisposeOTNotifyUPP(DNREventHandlerUPP);
			}				
		}
		
		if ( inetsvc ) {
			OTCloseProvider( inetsvc );
		}
	}
	return( err );
}

/*
 * return a ASCII equivalent of ipaddr.  addrstr must be at large enough to hold the
 * result which is of the form "AAA.BBB.CCC.DDD" and can be a maximum of 16 bytes in size
 */
short
ipaddr2str( InetHost ipaddr, char *addrstr )
{
	OSStatus				err;

	if ( gHaveOT ) {
	
		OTInetHostToString( ipaddr, addrstr );
		err = kOTNoError;
	
	}	
	return( err );
}


/*******************************************************************************
** EventHandler
********************************************************************************/
static pascal void EventHandler(void * contextPtr, OTEventCode event, OTResult result, void * /* cookie */)
{
	tcpstream *tsp	= (tcpstream *)contextPtr;
	Boolean enteredNotifier = OTEnterNotifier(tsp->tcps_ep);
	
	switch(event)
	{
   		// OTConnect callback		 		%%%
		case T_CONNECT:	
			if ( result == noErr )	{				
				tsp->tcps_connected = true;
			}
			break;

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
	
	if (enteredNotifier) {
		OTLeaveNotifier(tsp->tcps_ep);
	}
	
	return;
}

/*******************************************************************************
** Simplified EventHandler for DNR events
********************************************************************************/
static pascal void DNREventHandler(void * contextPtr, OTEventCode event, OTResult result, void * /* cookie */)
{
	switch(event)
	{
		// OTInetStringToAddress Complete	%%%
        case T_DNRSTRINGTOADDRCOMPLETE:
        // OTInetAddressToName Complete		%%%	
        case T_DNRADDRTONAMECOMPLETE:
        	{
        	int *done = (void *)contextPtr;
        	if ( result == noErr )
				*done = 1;
			else
				*done = 2;
			}
			break;
			
		default:
			break;
	}
}
