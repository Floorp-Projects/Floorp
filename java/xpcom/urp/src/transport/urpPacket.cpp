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
#include <nspr.h>
#include <prmem.h>
#include <prnetdb.h>

#include "urpPacket.h"

// Constructor/Destructor
urpPacket::urpPacket() {
    buff = NULL;
    buffSize = 0;
    messageCount = 0;
    point = 0;
}

urpPacket::~urpPacket() {
    PR_Free( buff );
}

// property section
void urpPacket::SetMessageCount( int msgCount ){
    messageCount = msgCount;
}

int urpPacket::GetMessageCount( void ){
    return messageCount;
}

// read/write section
void urpPacket::WriteBytes( void *bytes, int size ){
    if ( buffSize - point < size ) {
        buff = PR_Realloc( buff, buffSize += ((size<BUFSIZ)?BUFSIZ:size) );
    }
    memcpy( (char *)buff+point, bytes, size ); point += size;
}

//
// Write Section =======================================
//

void urpPacket::WriteByte( char byteValue ){
    WriteBytes( &byteValue, 1 );
}

void urpPacket::WriteBoolean( char boolValue ){
    WriteBytes( &boolValue, 1 );
}

// Write Section 2 bytes
void urpPacket::WriteShort( short shortValue ){
    shortValue = PR_htons( shortValue ); // host to network
    WriteBytes( &shortValue, 2 );
}

// Write Section 4 bytes
void urpPacket::WriteInt( int intValue ){
    intValue = PR_htons( intValue );
    WriteBytes( &intValue, 4 );
}

void urpPacket::WriteLong( long longValue ){
    longValue = PR_htons( longValue );
    WriteBytes( &longValue, 4 );
}

void urpPacket::WriteFloat( float floatValue ){
    WriteBytes( &floatValue, 4 );
}

// Write Section 8 bytes
void urpPacket::WriteDouble( double doubleValue ){
    WriteBytes( &doubleValue, 8 );
}

// Write Section arrays
void urpPacket::WriteOctetStream( char* stream, int size ){
    if ( size < 0xff ) {
        WriteByte( (char)size );
    }  else  { /* write compressed */
        WriteByte( 0xff );
        WriteLong( size );
    }
    WriteBytes( stream, size );
}


void urpPacket::WriteString( char* str, int size ) {
    // probably something to do about string
    // ...
    WriteOctetStream( str, size );
}

//
// Read Section =======================================
//

// read 1 byte
char urpPacket::ReadByte( void ) {
    return ((char *)buff)[point++];
}

char urpPacket::ReadBoolean( void ) {
    return ((char *)buff)[point++];
}

// read 2 bytes
short urpPacket::ReadShort( void ) {
    short retShort;
    memcpy( &retShort, ((char *)buff)+point, 2 ); point += 2;
    return PR_ntohs(retShort);
}

int urpPacket::ReadInt( void ) {
    int retInt;
    memcpy( &retInt, ((char *)buff)+point, 4 ); point += 4;
    return PR_ntohl(retInt);
}

long urpPacket::ReadLong( void ) {
    long retLong;
    memcpy( &retLong, ((char *)buff)+point, 4 ); point += 4;
    return PR_ntohl(retLong);
}

float urpPacket::ReadFloat( void ) {
    float retFloat;
    memcpy( &retFloat, ((char *)buff)+point, 4 ); point += 4;
    return retFloat;
}

// read 8 bytes
double urpPacket::ReadDouble( void ) {
    double retDouble;
    memcpy( &retDouble, ((char *)buff)+point, 8 ); point += 8;
    return retDouble;
}

char * urpPacket::ReadOctetStream( int& size ) {
    // read packed size 
    if( ((char *)buff)[point] == (char)0xff ) {
        point++; size = ReadInt();
    }
    else size = (int)ReadByte();

    // read array
    char *retStream = (char *)PR_Malloc( size+1 ); // add 1 byte for string needs
    memcpy( retStream, ((char *)buff)+point, size );
    point += size;
    return retStream;
}

char * urpPacket::ReadString( int& size ) {
    char * retString = ReadOctetStream( size );
    // probaly something to do about string (convert from UTF8)
    retString[size] = 0; // make string null-terminated
    // ...
    return retString;
}
