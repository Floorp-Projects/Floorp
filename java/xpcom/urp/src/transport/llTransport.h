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

#ifndef llTransport_included
#define llTransport_included

#include <nspr.h>

enum llSideType { llClient, llServer };

class llConnection {
public:
    virtual PRInt32 Read( void *buf, PRInt32 amount ) = 0;
    virtual PRInt32 Write( void *buf, PRInt32 amount ) = 0;
};

class llTransport {
protected:
    llSideType type;
public:
    char * ParseParam( char *in_str, char *param_name );
    virtual PRStatus Open( char *name, llSideType type ) = 0;
    virtual PRStatus Close( void ) = 0;
    virtual llConnection * ProvideConnection( void ) = 0;
};

class llSocketConnection : public llConnection {
    PRFileDesc *socket;
    friend class llTCPTransport;
public:
    ~llSocketConnection();
    PRInt32 Read( void *buf, PRInt32 amount );
    PRInt32 Write( void *buf, PRInt32 amount );
};

class llTCPTransport : public llTransport {
protected:
    PRFileDesc *fd; // only for server needs
    PRNetAddr addr; // only for client needs
public:
    PRStatus Open( char *name, llSideType type );
    PRStatus Close( void );
    llConnection * ProvideConnection( void );
};

class llPipeTransport  : public llTCPTransport {
public:
    PRStatus Open( char *name, llSideType type );
};

#endif // llTransport_included
