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

#ifndef __bcICall_h
#define __bcICall_h
#include "bcIMarshaler.h"
#include "bcIUnMarshaler.h"

class bcIORB;

class bcICall {
 public:
    virtual int GetParams(bcIID *, bcOID *, bcMID *) = 0;
    virtual bcIMarshaler * GetMarshaler() = 0;
    virtual bcIUnMarshaler * GetUnMarshaler() = 0;
    virtual bcIORB * GetORB() = 0;
};

#define INVOKE_ADDREF(oid,iid,orb) \
        do {                          \
            bcICall * call;           \
            call = GET_ADDREF_CALL((oid),(iid),(orb)); \
            (orb)->SendReceive(call);  \
            delete call;               \
        } while(0)

#define INVOKE_RELEASE(oid,iid,orb)   \
        do {                          \
            bcICall * call;           \
            call = GET_RELEASE_CALL((oid),(iid),(orb)); \
            (orb)->SendReceive(call);  \
            delete call;               \
        } while(0)

#define GET_ADDREF_CALL(oid,iid,orb) \
        (orb)->CreateCall((iid),(oid),1)

#define GET_RELEASE_CALL(oid,iid,orb) \
        (orb)->CreateCall((iid),(oid),2)
#endif











