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

#ifndef urpTransport_included
#define urpTransport_included

#include <nspr.h>

#include "llTransport.h"
#include "urpPacket.h"

enum urpConnectionStatus { urpSuccess, urpFailed };

class urpConnection {
    urpConnectionStatus status;
    llConnection *conn;
    friend class urpTransport;
public:
    urpConnection();
    ~urpConnection();
    urpConnectionStatus GetStatus( void );
    void Write( urpPacket *pkt );
    urpPacket * Read( void );
};

class urpTransport {
protected:
    llTransport *trans;
    llSideType type;

public:
    urpTransport();

    PRBool IsClient( void );
    PRBool IsServer( void );

    PRStatus Open( char* connectString );
    void Close( void );

    urpConnection * GetConnection( void );
};

class urpConnector : public urpTransport {
public:
    urpConnector() { type = llClient; }
};

class urpAcceptor : public urpTransport{
public:
    urpAcceptor() { type = llServer; }
};

#endif // UurpTransport_included
