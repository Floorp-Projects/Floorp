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

#ifndef urpPacket_included
#define urpPacket_included

class urpPacket {
    void *buff;
    int buffSize;
    int messageCount;
    int point;

    void WriteBytes( void *bytes, int size );

    friend class urpConnection;

public:

    urpPacket();
    ~urpPacket();

    int GetMessageCount( void );
    void SetMessageCount( int msgCount );

    // Write Section
    // 1 byte
    void WriteByte( char byteValue );
    void WriteBoolean( char boolValue );
    // 2 bytes
    void WriteShort( short shortValue );
    // 4 bytes
    void WriteInt( int intValue );
    void WriteLong( long longValue );
    void WriteFloat( float floatValue );
    // 8 bytes
    void WriteDouble( double doubleValue );
    // arrays
    void WriteOctetStream( char* stream, int size );
    void WriteString( char* str, int size );

    // Read Section
    char ReadByte( void );
    char ReadBoolean( void );
    short ReadShort( void );
    int ReadInt( void );
    long ReadLong( void );
    float ReadFloat( void );
    double ReadDouble( void );
    char * ReadOctetStream( int& size );
    char * ReadString( int& size );
};

#endif // urpPacket_included
