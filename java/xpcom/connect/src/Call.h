/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#ifndef __Call_h
#define __Call_h

#ifdef WIN32
#include <strstrea.h>
#else
#include <strstream.h>
#endif
#include "bcICall.h"

class Call : public bcICall {
public:
    Call();
    Call(bcIID *, bcOID *, bcMID, bcIORB *orb);
    virtual ~Call();
    virtual int GetParams(bcIID *, bcOID *, bcMID*);
    virtual bcIORB * GetORB();
    virtual bcIMarshaler * GetMarshaler();
    virtual bcIUnMarshaler * GetUnMarshaler();
private :
    ostrstream *out;
    istrstream *in;
    bcIID iid;
    bcOID oid;
    bcMID mid;
    bcIORB *orb;
};

#endif
