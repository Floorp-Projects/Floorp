/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
 * See the License for the specific language governing rights and
 * limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun are 
 * Copyright (C) 1999 Sun Microsystems, Inc.
 * All Rights Reserved.
 * 
 * Contributor(s):
 * Serge Pikalev <sep@sparc.spb.su>
 *
 */

#include <stdio.h>
#include <string.h>

#include "llTransport.h"
#include "private/pprio.h"

// ========== llTransport def ==========

char * llTransport::ParseParam( char *in_str, char *param_name )
{
    // move point to "=<value>,<something>" string
    char *value = strstr( in_str, param_name ) + strlen(param_name);
    if( (value == NULL) || (*value != '=') ) { 
	return NULL;
    }
    value++; // move to "<value>,<something>" string
    // extract value
    char *valueend = strpbrk( value, ",;" );
    if( valueend == NULL ) for( valueend = value; *valueend != 0; valueend++ );
    int valuesize = valueend - value;
    char *retvalue = (char*)PR_Malloc( valuesize + 1 );
    memcpy( retvalue, value, valuesize ); retvalue[valuesize] = 0;

    return retvalue;
}

// ========== llSocketConnection ==========

llSocketConnection::~llSocketConnection()
{
    PR_Close(socket);
}

PRInt32 llSocketConnection::Read( void *buf, PRInt32 amount )
{
    return PR_Recv( socket, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT );
}

PRInt32 llSocketConnection::Write( void *buf, PRInt32 amount )
{
    return PR_Send( socket, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT );
}

// ========== llTCPTransport ==========

static PRStatus fill_addr_by_name( char *hostname,
                                   PRUint16 port,
                                   PRNetAddr *addr )
{
    PRStatus status;
    PRIntn result;
    PRHostEnt hostentry;
    char buf[PR_NETDB_BUF_SIZE];

    status = PR_GetHostByName( hostname, buf, PR_NETDB_BUF_SIZE, &hostentry );
    if( status == PR_FAILURE ) {
	return PR_FAILURE;
    }

    result = PR_EnumerateHostEnt( 0, &hostentry, port, addr );
    if( result == 0 ) {
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}

static PRFileDesc *create_socket( PRUint16 family )
{
    switch( family )
    {
        case AF_INET: return PR_NewTCPSocket();
        case AF_UNIX: return (PRFileDesc *)PR_Socket( AF_UNIX, SOCK_STREAM, 0 );
    }
    return NULL;
}

PRStatus llTCPTransport::Open( char *name, llSideType type )
{
    PRStatus status;
    PRSocketOptionData sopt;

    this->type = type;

    char *host = ParseParam( name, "host" );
    char *portStr = ParseParam( name, "port" );
    PRUint16 port = atoi( portStr );

    if( type==llServer ) // prepare to accept
    {
        fd = create_socket( AF_INET );
        if ( fd == NULL ) {
            status = PR_FAILURE;
        }
        else
        {
            sopt.option = PR_SockOpt_Reuseaddr;
            sopt.value.reuse_addr = PR_TRUE;
            PR_SetSocketOption( fd, &sopt );

            addr.inet.family = AF_INET;
            addr.inet.port = port;
            addr.inet.ip = PR_htonl(INADDR_ANY);
            status = PR_Bind( fd, &addr );
            if( status == PR_SUCCESS ) PR_Listen( fd, 5 );
        }
        PR_Free( host );
        PR_Free( portStr );
        return status;
    }
    if( type == llClient ) // prepare to connect
    {
        status = fill_addr_by_name( host, port, &addr );
        // free allocated mem
        PR_Free( host );
        PR_Free( portStr );
        return status;
    }
    return PR_FAILURE;
}

PRStatus llTCPTransport::Close( void )
{
    return PR_Close( fd );
}

llConnection * llTCPTransport::ProvideConnection( void )
{
    PRSocketOptionData sopt;
    PRFileDesc *socket;

    if( type == llServer )
    {
        // fd is constant
        socket = PR_Accept( fd, &addr, PR_INTERVAL_NO_TIMEOUT );
        if ( socket == NULL ) {
            return NULL;
        }
    } else
    if( type == llClient )
    {
        // addr is constant
        socket = create_socket( addr.raw.family );
        if ( socket == NULL ) {
            return NULL;
        }
        PRStatus status = PR_Connect( socket, &addr, PR_INTERVAL_NO_TIMEOUT );
        if ( status == PR_FAILURE ) {
            return NULL;
        }
    }

    // set option to blocking read/write ops
    sopt.option = PR_SockOpt_Nonblocking;
    sopt.value.non_blocking = PR_FALSE;
    PR_SetSocketOption( socket, &sopt );

    llSocketConnection *cnt = new llSocketConnection();
    cnt->socket = socket;
    return cnt;
}

// ========== llPipeTransport ==========

PRStatus llPipeTransport::Open( char *name, llSideType type )
{
    PRStatus status;
    PRSocketOptionData sopt;

    this->type = type;

    const char prefix[] = "/tmp/OSL_PIPE_";
    char *path = ParseParam( name, "name" );

    addr.local.family = AF_UNIX;
    strcpy( addr.local.path, prefix );
    strcpy( addr.local.path+sizeof(prefix)-1, path );
    PR_Free( path );

    if( type == llServer ) // prepare to accept
    {
        fd = create_socket( AF_UNIX );
        if( fd == NULL ) return PR_FAILURE;

        sopt.option = PR_SockOpt_Reuseaddr;
        sopt.value.reuse_addr = PR_TRUE;
        PR_SetSocketOption( fd, &sopt );

        status = PR_Bind( fd, &addr );
        if ( status == PR_FAILURE ) {
            return PR_FAILURE;
        }

        PR_Listen( fd, 5 );
        return PR_SUCCESS;
    }
    if ( type == llClient ) {
        return PR_SUCCESS;
    }

    return PR_FAILURE;
}
