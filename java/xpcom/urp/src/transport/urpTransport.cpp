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
#include <nspr.h>
#include <prnetdb.h>

#include "llTransport.h"
#include "urpPacket.h"
#include "urpTransport.h"

struct urpPacketHeader {
    PRUint32 blockSize;
    PRUint32 messageCount;
};

// ========== urpConnection ==========

urpConnection::urpConnection(){
    status = urpSuccess;
}

urpConnection::~urpConnection(){
    delete conn;
}

urpConnectionStatus urpConnection::GetStatus( void ){
    return status;
}

void urpConnection::Write( urpPacket *pkt ){
    urpPacketHeader hdr;
    PRInt32 size;

    hdr.blockSize = PR_htonl(pkt->point);
    hdr.messageCount = PR_htonl(pkt->messageCount);
    // use transport
    conn->Write( &hdr, sizeof(urpPacketHeader) );
    size = conn->Write( pkt->buff, pkt->point );
    if ( size < pkt->point ) {
        status = urpFailed;
    }
}

urpPacket * urpConnection::Read( void ){
    urpPacketHeader hdr;
    urpPacket *pkt = new urpPacket();
    PRInt32 size;

    // use transport
    conn->Read( &hdr, sizeof(urpPacketHeader) ); // read header
    pkt->buffSize = PR_ntohl( hdr.blockSize );;
    pkt->messageCount = PR_ntohl( hdr.messageCount );
    pkt->buff = PR_Malloc( pkt->buffSize );
    size = conn->Read( pkt->buff, pkt->buffSize );
    if ( size < pkt->buffSize ) {
        status = urpFailed;
    }

    return pkt;
}

// ========== urpTransport ==========
urpTransport::urpTransport(){
    trans = NULL;
}

PRBool urpTransport::IsClient( void ){
    return (type == llClient);
}

PRBool urpTransport::IsServer( void ){
    return (type == llServer);
}

PRStatus urpTransport::Open( char* connectString ){
    // parse lowlevel protocol
    char *comma = strchr( connectString, ',' );
    if( strncmp( "socket", connectString, comma-connectString ) == 0 ) {
        trans = new llTCPTransport();
    }
    else if( strncmp( "pipe", connectString, comma-connectString ) == 0 ) {
        trans = new llPipeTransport();
    }
    else return PR_FAILURE;

    return trans->Open( connectString, type );
}

void urpTransport::Close( void ){
    trans->Close();
    delete trans;
}

urpConnection * urpTransport::GetConnection( void ){
    llConnection *conn = trans->ProvideConnection();
    if ( conn == NULL ) {
        return NULL;
    }
    urpConnection *uConn = new urpConnection();
    uConn->conn = conn;
    return uConn;
}
